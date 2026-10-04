use crate::error::{Error, Result};
use serde::Deserialize;
use std::io::{self, BufRead, IsTerminal, Write};
use zeroize::{Zeroize, ZeroizeOnDrop, Zeroizing};

#[derive(Default, Deserialize, Zeroize, ZeroizeOnDrop)]
pub struct Secrets {
    pub mnemonic: Option<String>,
    pub passphrase: Option<String>,
}
impl Secrets {
    pub fn read() -> Result<Self> {
        let invalid = || {
            Error::new(
                "invalid_secret_input",
                "expected one JSON object containing mnemonic and/or passphrase strings",
            )
        };
        if io::stdin().is_terminal() {
            return Err(Error::new(
                "invalid_secret_input",
                "--input-stdin requires a private pipe or redirected input",
            ));
        }
        let mut input = Zeroizing::new(Vec::new());
        let mut stdin = io::stdin().lock();
        loop {
            let bytes = stdin.fill_buf()?;
            if bytes.is_empty() {
                break;
            }
            let end = bytes.iter().position(|b| *b == b'\n');
            let n = end.unwrap_or(bytes.len());
            if input.len() + n > 16384 {
                return Err(invalid());
            }
            input.extend_from_slice(&bytes[..n]);
            stdin.consume(n + usize::from(end.is_some()));
            if end.is_some() {
                break;
            }
        }
        // Deserialize directly so malformed input never appears in diagnostics.
        serde_json::from_slice(&input).map_err(|_| invalid())
    }
    pub fn value(&self, name: &'static str, non_interactive: bool) -> Result<Zeroizing<String>> {
        let value = if name == "mnemonic" {
            &self.mnemonic
        } else {
            &self.passphrase
        };
        if let Some(s) = value {
            return Ok(Zeroizing::new(s.clone()));
        }
        if non_interactive {
            return Err(Error::new(
                if name == "mnemonic" {
                    "mnemonic_required"
                } else {
                    "passphrase_required"
                },
                format!("{name} must be supplied through --input-stdin"),
            ));
        }
        password(&format!("{name}: "))
    }
}
#[cfg(unix)]
struct EchoGuard(libc::termios);
#[cfg(unix)]
impl Drop for EchoGuard {
    fn drop(&mut self) {
        unsafe {
            libc::tcsetattr(libc::STDIN_FILENO, libc::TCSANOW, &self.0);
        }
    }
}
pub fn password(prompt: &str) -> Result<Zeroizing<String>> {
    eprint!("{prompt}");
    io::stderr().flush()?;
    #[cfg(unix)]
    let guard = if io::stdin().is_terminal() {
        let mut old = std::mem::MaybeUninit::<libc::termios>::uninit();
        // POSIX initializes the structure on success; the guard restores echo
        // on every normal/error return from the password read.
        if unsafe { libc::tcgetattr(libc::STDIN_FILENO, old.as_mut_ptr()) } != 0 {
            return Err(io::Error::last_os_error().into());
        }
        let old = unsafe { old.assume_init() };
        let mut private = old;
        private.c_lflag &= !libc::ECHO;
        if unsafe { libc::tcsetattr(libc::STDIN_FILENO, libc::TCSANOW, &private) } != 0 {
            return Err(io::Error::last_os_error().into());
        }
        Some(EchoGuard(old))
    } else {
        None
    };
    #[cfg(not(unix))]
    if io::stdin().is_terminal() {
        return Err(Error::new(
            "secret_input_unavailable",
            "use --input-stdin for private secret input on this platform",
        ));
    }
    let mut value = Zeroizing::new(String::new());
    io::stdin().read_line(&mut value)?;
    while value.ends_with(['\r', '\n']) {
        value.pop();
    }
    #[cfg(unix)]
    drop(guard);
    eprintln!();
    Ok(value)
}
