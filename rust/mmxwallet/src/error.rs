use std::fmt;
pub type Result<T> = std::result::Result<T, Error>;
#[derive(Debug)]
pub struct Error {
    pub code: &'static str,
    pub message: String,
}
impl Error {
    pub fn new(code: &'static str, message: impl Into<String>) -> Self {
        Self {
            code,
            message: message.into(),
        }
    }
}
impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.message)
    }
}
impl From<std::io::Error> for Error {
    fn from(e: std::io::Error) -> Self {
        Self::new("io_error", e.to_string())
    }
}
impl From<mmx_wallet::Error> for Error {
    fn from(e: mmx_wallet::Error) -> Self {
        let code = match e {
            mmx_wallet::Error::InvalidArgument(_) => "invalid_argument",
            mmx_wallet::Error::InvalidAmount(_) => "invalid_amount",
            mmx_wallet::Error::InvalidMnemonic => "invalid_mnemonic",
            mmx_wallet::Error::InvalidPassphrase => "invalid_passphrase",
            mmx_wallet::Error::InsufficientFunds(_) => "insufficient_funds",
            _ => "wallet_error",
        };
        Self::new(code, e.to_string())
    }
}
pub fn invalid(message: impl Into<String>) -> Error {
    Error::new("invalid_argument", message)
}
pub fn rpc_invalid(message: impl Into<String>) -> Error {
    Error::new("rpc_response_invalid", message)
}
