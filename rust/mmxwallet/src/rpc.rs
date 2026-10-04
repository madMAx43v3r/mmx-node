use crate::error::{invalid, rpc_invalid, Error, Result};
use mmx_wallet::{Address, ChainParams};
use serde_json::Value;
use std::time::Duration;
const MAX_RESPONSE: u64 = 16 * 1024 * 1024;
pub struct Rpc {
    agent: ureq::Agent,
    base: String,
}
fn transport(e: ureq::Error) -> Error {
    let code = match e {
        ureq::Error::Timeout(_) => "rpc_timeout",
        ureq::Error::BodyExceedsLimit(_) => "rpc_response_invalid",
        _ => "rpc_transport_error",
    };
    Error::new(code, "RPC request failed; a submitted transaction may still have been accepted; check its ID before retrying")
}
impl Rpc {
    pub fn new(url: &str) -> Result<Self> {
        let mut base = url.trim().trim_end_matches('/').to_owned();
        if !base.contains("://") {
            base = format!("https://{base}");
        }
        let uri: ureq::http::Uri = base.parse().map_err(|_| invalid("invalid RPC URL"))?;
        if !matches!(uri.scheme_str(), Some("http" | "https"))
            || uri.host().is_none()
            || base.chars().any(char::is_control)
        {
            return Err(invalid("RPC URL must use HTTP or HTTPS"));
        }
        let agent = ureq::Agent::config_builder()
            .timeout_global(Some(Duration::from_secs(30)))
            .timeout_connect(Some(Duration::from_secs(10)))
            .max_redirects(0)
            .max_redirects_will_error(false)
            .http_status_as_error(false)
            .build()
            .new_agent();
        Ok(Self { agent, base })
    }
    fn request(&self, path: &str, body: Option<&[u8]>) -> Result<Vec<u8>> {
        let url = format!("{}{path}", self.base);
        let mut response = match body {
            Some(b) => self
                .agent
                .post(&url)
                .header("Content-Type", "application/json")
                .send(b),
            None => self.agent.get(&url).call(),
        }
        .map_err(transport)?;
        let status = response.status().as_u16();
        let bytes = response
            .body_mut()
            .with_config()
            .limit(MAX_RESPONSE)
            .read_to_vec()
            .map_err(transport)?;
        if !(200..300).contains(&status) {
            let detail: String = String::from_utf8_lossy(&bytes).chars().take(500).collect();
            return Err(Error::new(
                "rpc_http_error",
                format!("RPC returned HTTP {status}: {detail}"),
            ));
        }
        Ok(bytes)
    }
    pub fn get(&self, path: &str) -> Result<Value> {
        let bytes = self.request(path, None)?;
        if bytes.is_empty() {
            return Ok(Value::Null);
        }
        serde_json::from_slice(&bytes).map_err(|_| rpc_invalid("invalid JSON response from RPC"))
    }
    pub fn post(&self, path: &str, bytes: &[u8]) -> Result<()> {
        self.request(path, Some(bytes))?;
        Ok(())
    }
    pub fn validate(&self, bytes: &[u8], max_fee: u32) -> Result<u128> {
        let v: Value = serde_json::from_slice(&self.request("/transaction/validate", Some(bytes))?)
            .map_err(|_| rpc_invalid("invalid JSON response from RPC"))?;
        if boolean(&v, "did_fail")? {
            return Err(Error::new(
                "wallet_error",
                format!("transaction execution would fail: {}", v["error"]),
            ));
        }
        let fee = atomic(&v["total_fee"])?;
        if fee > max_fee as u128 {
            return Err(rpc_invalid("RPC returned a fee above the signed maximum"));
        }
        Ok(fee)
    }
    pub fn params(&self) -> Result<ChainParams> {
        let v = self.get("/chain/info")?;
        if v["network"].as_str().is_none_or(str::is_empty) {
            return Err(rpc_invalid("chain parameters missing network"));
        }
        let params: ChainParams =
            serde_json::from_value(v).map_err(|_| rpc_invalid("invalid chain parameters"))?;
        if params.decimals > 18 {
            return Err(rpc_invalid("invalid native currency decimals"));
        }
        Ok(params)
    }
    pub fn height(&self, params: &ChainParams) -> Result<u32> {
        let node = self.get("/node/info")?;
        if !boolean(&node, "is_synced")? {
            return Err(Error::new("rpc_not_synced", "RPC node is not synced"));
        }
        if text(&node, "name")? != params.network {
            return Err(Error::new(
                "rpc_network_mismatch",
                "RPC network does not match chain parameters",
            ));
        }
        u32_field(&node, "height")
    }
}
pub fn text<'a>(v: &'a Value, field: &str) -> Result<&'a str> {
    v[field]
        .as_str()
        .ok_or_else(|| rpc_invalid(format!("invalid RPC field: {field}")))
}
pub fn u32_field(v: &Value, field: &str) -> Result<u32> {
    v[field]
        .as_u64()
        .and_then(|n| n.try_into().ok())
        .ok_or_else(|| rpc_invalid(format!("invalid RPC field: {field}")))
}
pub fn boolean(v: &Value, field: &str) -> Result<bool> {
    v[field]
        .as_bool()
        .ok_or_else(|| rpc_invalid(format!("invalid RPC field: {field}")))
}
pub fn atomic(v: &Value) -> Result<u128> {
    match v {
        Value::String(s) => s.parse().ok(),
        Value::Number(n) => n.to_string().parse().ok(),
        _ => None,
    }
    .ok_or_else(|| rpc_invalid("invalid 128-bit atomic amount"))
}
pub fn address(v: &Value, field: &str) -> Result<Address> {
    text(v, field)?
        .parse()
        .map_err(|_| rpc_invalid("invalid address from RPC"))
}
pub fn decimals(v: &Value, currency: Address, params: &ChainParams) -> Result<u32> {
    let decimals = u32_field(v, "decimals")?;
    if decimals > 18 || (currency == Address::default() && decimals != params.decimals) {
        return Err(rpc_invalid("inconsistent currency decimals"));
    }
    Ok(decimals)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::{io::Read, net::TcpListener, thread};

    #[test]
    fn stalled_http_request_returns_structured_timeout() {
        let server = TcpListener::bind("127.0.0.1:0").unwrap();
        let address = server.local_addr().unwrap();
        let handle = thread::spawn(move || {
            let (mut stream, _) = server.accept().unwrap();
            let mut request = [0; 1024];
            assert!(stream.read(&mut request).unwrap() > 0);
            // Keep the connection open without responding, exercising the
            // actual client's deadline rather than simulating an error enum.
            thread::sleep(Duration::from_millis(200));
        });
        let rpc = Rpc {
            base: format!("http://{address}"),
            agent: ureq::Agent::config_builder()
                .proxy(None)
                .timeout_global(Some(Duration::from_millis(50)))
                .build()
                .new_agent(),
        };
        let error = rpc.get("/node/info").unwrap_err();
        handle.join().unwrap();
        assert_eq!(error.code, "rpc_timeout");
    }
}
