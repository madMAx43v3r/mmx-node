use crate::{
    error::{invalid, rpc_invalid, Result},
    rpc::{self, Rpc},
};
use mmx_wallet::{format_amount, Address, ChainParams, Wallet};
use serde_json::{json, Value};
use std::collections::BTreeMap;

pub struct Balance {
    pub amount: u128,
    pub decimals: u32,
    pub symbol: String,
}
pub struct State {
    pub height: u32,
    pub balances: BTreeMap<(Address, Address), u128>,
    pub totals: BTreeMap<Address, Balance>,
}
impl State {
    pub fn fetch(rpc: &Rpc, wallet: &Wallet, params: &ChainParams) -> Result<Self> {
        let height = rpc.height(params)?;
        let mut balances = BTreeMap::new();
        let mut totals: BTreeMap<Address, Balance> = BTreeMap::new();
        for address in &wallet.addresses {
            let v = rpc.get(&format!("/address?id={address}&limit=1000"))?;
            for row in v["balances"]
                .as_array()
                .ok_or_else(|| rpc_invalid("invalid balances response"))?
            {
                let currency = rpc::address(row, "contract")?;
                // NFT holdings have no fungible currency metadata in the node
                // response and are not part of this CLI's currency balances.
                if row["is_nft"].as_bool() == Some(true) {
                    continue;
                }
                let amount = rpc::atomic(&row["amount"])?;
                let decimals = rpc::decimals(row, currency, params)?;
                let symbol = rpc::text(row, "symbol")?.to_owned();
                if balances.insert((*address, currency), amount).is_some() {
                    return Err(rpc_invalid("duplicate balance"));
                }
                if let Some(total) = totals.get_mut(&currency) {
                    if total.decimals != decimals || total.symbol != symbol {
                        return Err(rpc_invalid("inconsistent currency metadata"));
                    }
                    total.amount = total
                        .amount
                        .checked_add(amount)
                        .ok_or_else(|| rpc_invalid("balance total exceeds 128 bits"))?;
                } else {
                    totals.insert(
                        currency,
                        Balance {
                            amount,
                            decimals,
                            symbol,
                        },
                    );
                }
            }
        }
        Ok(Self {
            height,
            balances,
            totals,
        })
    }
    pub fn rows(&self, filter: &Filter, params: &ChainParams) -> Result<Vec<Value>> {
        let mut rows = Vec::new();
        for (currency, total) in &self.totals {
            if filter.matches(*currency, &total.symbol) {
                rows.push(json!({"currency_address":currency, "symbol":total.symbol, "decimals":total.decimals,
                    "amount_atomic":total.amount.to_string(), "amount":format_amount(total.amount, total.decimals)?}));
            }
        }
        if rows.is_empty() {
            match filter {
                Filter::Symbol(_) => return Err(invalid("no currencies match symbol")),
                Filter::Currency(c) if *c != Address::default() => (),
                _ => rows.push(json!({"currency_address":Address::default(), "symbol":"MMX", "decimals":params.decimals, "amount_atomic":"0", "amount":"0"})),
            }
        }
        Ok(rows)
    }
}
pub enum Filter {
    All,
    Currency(Address),
    Symbol(String),
}
impl Filter {
    pub fn parse(s: &str) -> Result<Self> {
        Ok(match s {
            "all" => Self::All,
            "" | "MMX" => Self::Currency(Address::default()),
            s if s.starts_with("mmx1") => Self::Currency(s.parse()?),
            _ => Self::Symbol(s.into()),
        })
    }
    fn matches(&self, currency: Address, symbol: &str) -> bool {
        match self {
            Self::All => true,
            Self::Currency(c) => *c == currency,
            Self::Symbol(s) => s == symbol,
        }
    }
}
pub fn currency(s: &str) -> Result<Address> {
    match s {
        "" | "MMX" => Ok(Address::default()),
        s if s.starts_with("mmx1") => Ok(s.parse()?),
        _ => Err(invalid("sending a token requires its contract address")),
    }
}
pub fn history(
    rpc: &Rpc,
    wallet: &Wallet,
    params: &ChainParams,
    filter: &Filter,
    limit: u32,
) -> Result<Vec<Value>> {
    let mut rows = Vec::new();
    for address in &wallet.addresses {
        let mut until = None;
        let mut matches = 0usize;
        loop {
            let request_limit = if matches!(filter, Filter::Symbol(_)) {
                1000
            } else {
                limit
            };
            let mut path = format!("/address/history?id={address}&limit={request_limit}");
            if let Filter::Currency(c) = filter {
                path.push_str(&format!("&currency={c}"));
            }
            if let Some(h) = until {
                path.push_str(&format!("&until={h}"));
            }
            let v = rpc.get(&path)?;
            let values = v
                .as_array()
                .ok_or_else(|| rpc_invalid("invalid history response"))?;
            if values.is_empty() {
                break;
            }
            let mut min_height = u32::MAX;
            for row in values {
                let c = rpc::address(row, "contract")?;
                let h = if row["height"].is_null() && rpc::boolean(row, "is_pending")? {
                    0
                } else {
                    rpc::u32_field(row, "height")?
                };
                min_height = min_height.min(h);
                if let Filter::Currency(want) = filter {
                    if *want != c {
                        return Err(rpc_invalid("history for wrong currency"));
                    }
                }
                if !filter.matches(c, rpc::text(row, "symbol")?) {
                    continue;
                }
                let decimals = rpc::decimals(row, c, params)?;
                let amount = rpc::atomic(&row["amount"])?;
                let mut row = row.clone();
                row["amount_atomic"] = json!(amount.to_string());
                row["amount"] = json!(format_amount(amount, decimals)?);
                rows.push(row);
                matches += 1;
            }
            if !matches!(filter, Filter::Symbol(_))
                || matches >= limit as usize
                || values.len() < request_limit as usize
                || min_height == 0
            {
                break;
            }
            let next = min_height - 1;
            if until.is_some_and(|h| next >= h) {
                return Err(rpc_invalid("history pagination made no progress"));
            }
            until = Some(next);
        }
    }
    rows.sort_by_key(|row| {
        std::cmp::Reverse((
            row["is_pending"].as_bool().unwrap_or(false),
            row["height"].as_u64().unwrap_or(0),
            row["time_stamp"].as_i64().unwrap_or(0),
        ))
    });
    rows.truncate(limit as usize);
    Ok(rows)
}
