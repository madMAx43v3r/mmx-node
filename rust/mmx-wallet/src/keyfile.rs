use crate::{fingerprint, hex_bytes, Error, Result, Wallet};
use rand::RngCore;
use serde::{Deserialize, Serialize};
use zeroize::{Zeroize, ZeroizeOnDrop};

const TYPE_HASH: u64 = 0xdf868931a939cba1;
const CODE_HASH: u64 = 0x35062c98e95706b2;
/// The compatible, unencrypted MMX seed container. A passphrase changes key
/// derivation and the stored fingerprint; it does not encrypt the seed.
#[derive(Serialize, Deserialize, Zeroize, ZeroizeOnDrop)]
#[serde(deny_unknown_fields)]
pub struct KeyFile {
    #[serde(rename = "__type", default = "type_name")]
    type_name: String,
    #[serde(with = "hex_bytes")]
    pub seed_value: [u8; 32],
    pub finger_print: Option<String>,
}
fn type_name() -> String {
    "mmx.KeyFile".into()
}
impl KeyFile {
    pub fn new(seed: [u8; 32], passphrase: Option<&str>) -> Self {
        Self {
            type_name: type_name(),
            seed_value: seed,
            finger_print: passphrase.map(|p| fingerprint(&seed, Some(p))),
        }
    }
    pub fn generate(passphrase: Option<&str>) -> Self {
        let mut seed = [0; 32];
        rand::rngs::OsRng.fill_bytes(&mut seed);
        Self::new(seed, passphrase)
    }
    pub fn fingerprint(&self) -> String {
        self.finger_print
            .clone()
            .unwrap_or_else(|| fingerprint(&self.seed_value, None))
    }
    pub fn requires_passphrase(&self) -> bool {
        self.finger_print
            .as_ref()
            .is_some_and(|f| *f != fingerprint(&self.seed_value, None))
    }
    pub fn unlock(&self, passphrase: &str, account: u32, count: u32) -> Result<Wallet> {
        if self.requires_passphrase()
            && self.finger_print.as_deref()
                != Some(&fingerprint(&self.seed_value, Some(passphrase)))
        {
            return Err(Error::InvalidPassphrase);
        }
        Wallet::derive(&self.seed_value, passphrase, account, count)
    }
    pub fn decode(data: &[u8]) -> Result<Self> {
        if data.len() > 65536 {
            return Err(Error::InvalidKeyFile);
        }
        if data.first() != Some(&255) {
            let key: Self = serde_json::from_slice(data).map_err(|_| Error::InvalidKeyFile)?;
            if key.type_name != type_name() || key.seed_value == [0; 32] {
                return Err(Error::InvalidKeyFile);
            }
            return Ok(key);
        }
        decode_binary(data)
    }
    /// Emit the same VNX v2 KeyFile schema as the node, implemented in Rust.
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        for n in [0xffff, 0x3713, 14, 2, 13] {
            u16le(&mut out, n);
        }
        for n in [TYPE_HASH, CODE_HASH, 0] {
            out.extend(n.to_le_bytes());
        }
        out.extend([0; 8]); // parents / dependencies
        string(&mut out, "mmx.KeyFile");
        out.extend(2u32.to_le_bytes());
        for (name, code) in [
            ("seed_value", &[11, 32, 1][..]),
            ("finger_print", &[33, 32][..]),
        ] {
            u16le(&mut out, 2);
            u16le(&mut out, 5);
            out.push(1);
            string(&mut out, name);
            out.extend((code.len() as u32).to_le_bytes());
            for c in code {
                u16le(&mut out, *c);
            }
            string(&mut out, "");
            u16le(&mut out, 1);
            u16le(&mut out, 1);
            out.push(0); // dynamic data_size = uint8(0)
        }
        out.extend([0; 12]); // static fields / methods / enums
        for n in [4, 13, 3, 32, 32] {
            u16le(&mut out, n);
        }
        out.extend([0; 4]); // empty dynamic alias map
        for n in [3, 11, 4, 31] {
            u16le(&mut out, n);
        }
        out.extend([0, 1, 0, 0]);
        u16le(&mut out, 1);
        u16le(&mut out, 32);
        string(&mut out, "");
        u16le(&mut out, 15);
        out.extend(CODE_HASH.to_le_bytes());
        out.extend(self.seed_value);
        out.push(u8::from(self.finger_print.is_some()));
        if let Some(f) = &self.finger_print {
            string(&mut out, f);
        }
        out
    }
}
fn u16le(out: &mut Vec<u8>, n: u16) {
    out.extend(n.to_le_bytes());
}
fn string(out: &mut Vec<u8>, s: &str) {
    out.extend((s.len() as u32).to_le_bytes());
    out.extend(s.as_bytes());
}

// A deliberately bounded decoder for KeyFile schemas, not a VNX runtime.
// Field names/types come from the on-disk schema, so seed-only legacy files
// and modern files with an optional fingerprint share the same reader.
struct Reader<'a> {
    data: &'a [u8],
    pos: usize,
    big: bool,
}
impl<'a> Reader<'a> {
    fn take(&mut self, n: usize) -> Result<&'a [u8]> {
        let end = self
            .pos
            .checked_add(n)
            .filter(|n| *n <= self.data.len())
            .ok_or(Error::InvalidKeyFile)?;
        let bytes = &self.data[self.pos..end];
        self.pos = end;
        Ok(bytes)
    }
    fn n(&mut self, width: usize) -> Result<u64> {
        let big = self.big;
        let b = self.take(width)?;
        Ok(if big {
            b.iter().fold(0, |n, b| (n << 8) | u64::from(*b))
        } else {
            b.iter().rev().fold(0, |n, b| (n << 8) | u64::from(*b))
        })
    }
    fn count(&mut self) -> Result<usize> {
        let n = self.n(4)? as usize;
        if n > 4096 {
            Err(Error::InvalidKeyFile)
        } else {
            Ok(n)
        }
    }
    fn text(&mut self) -> Result<String> {
        let n = self.count()?;
        String::from_utf8(self.take(n)?.to_vec()).map_err(|_| Error::InvalidKeyFile)
    }
    fn codes(&mut self, width: usize) -> Result<Vec<u16>> {
        let n = self.n(width)? as usize;
        if !(1..=32).contains(&n) {
            return Err(Error::InvalidKeyFile);
        }
        (0..n).map(|_| self.n(2).map(|n| n as u16)).collect()
    }
    fn dynamic(&mut self) -> Result<()> {
        let codes = self.codes(2)?;
        self.skip(&codes, 0)
    }
    fn skip(&mut self, code: &[u16], depth: usize) -> Result<()> {
        if depth > 8 || code.is_empty() {
            return Err(Error::InvalidKeyFile);
        }
        let c = code[0];
        match c {
            0 => (),
            1 | 5 | 31 => {
                self.take(1)?;
            }
            2 | 6 => {
                self.take(2)?;
            }
            3 | 7 | 9 => {
                self.take(4)?;
            }
            4 | 8 | 10 => {
                self.take(8)?;
            }
            32 => {
                self.text()?;
            }
            11 | 12 => {
                let n = if c == 11 {
                    *code.get(1).ok_or(Error::InvalidKeyFile)? as usize
                } else {
                    self.count()?
                };
                let start = if c == 11 { 2 } else { 1 };
                for _ in 0..n {
                    self.skip(&code[start..], depth + 1)?;
                }
            }
            13 => {
                let split = *code.get(1).ok_or(Error::InvalidKeyFile)? as usize;
                if split < 3 || split >= code.len() {
                    return Err(Error::InvalidKeyFile);
                }
                for _ in 0..self.count()? {
                    self.skip(&code[2..split], depth + 1)?;
                    self.skip(&code[split..], depth + 1)?;
                }
            }
            33 => {
                if self.n(1)? != 0 {
                    self.skip(&code[1..], depth + 1)?;
                }
            }
            _ => return Err(Error::InvalidKeyFile),
        }
        Ok(())
    }
    fn fields(&mut self) -> Result<Vec<(bool, String, Vec<u16>)>> {
        let mut fields = Vec::new();
        for _ in 0..self.count()? {
            let version = self.n(2)?;
            let count = match version {
                1 => 4,
                2 => self.n(2)?,
                _ => return Err(Error::InvalidKeyFile),
            };
            if !(4..=16).contains(&count) {
                return Err(Error::InvalidKeyFile);
            }
            let extended = self.n(1)? != 0;
            let name = self.text()?;
            let codes = self.codes(4)?;
            self.text()?;
            for _ in 4..count {
                self.dynamic()?;
            }
            fields.push((extended, name, codes));
        }
        Ok(fields)
    }
}
fn decode_binary(data: &[u8]) -> Result<KeyFile> {
    let mut r = Reader {
        data,
        pos: 0,
        big: false,
    };
    if r.n(2)? != 0xffff {
        return Err(Error::InvalidKeyFile);
    }
    r.big = match r.n(2)? {
        0x3713 => false,
        0x1337 => true,
        _ => return Err(Error::InvalidKeyFile),
    };
    if r.n(2)? != 14 {
        return Err(Error::InvalidKeyFile);
    }
    let version = r.n(2)?;
    let count = match version {
        1 => 10,
        2 => r.n(2)?,
        _ => return Err(Error::InvalidKeyFile),
    };
    if !(10..=16).contains(&count) || r.n(8)? != TYPE_HASH {
        return Err(Error::InvalidKeyFile);
    }
    let hash = r.n(8)?;
    r.n(8)?;
    // KeyFile has no inherited fields or dependencies.
    if r.count()? != 0 || r.count()? != 0 || r.text()? != "mmx.KeyFile" {
        return Err(Error::InvalidKeyFile);
    }
    let fields = r.fields()?;
    r.fields()?;
    for _ in 0..r.count()? {
        r.n(8)?;
    }
    for _ in 0..r.count()? {
        r.n(4)?;
        r.text()?;
    }
    for _ in 10..count {
        r.dynamic()?;
    }
    if r.n(2)? != 15 || r.n(8)? != hash {
        return Err(Error::InvalidKeyFile);
    }
    let mut key = KeyFile::new([0; 32], None);
    // VNX writes inline fields first and extended fields second.
    for extended in [false, true] {
        for (ext, name, code) in &fields {
            if *ext != extended {
                continue;
            }
            match (name.as_str(), code.as_slice()) {
                ("seed_value", [11, 32, 1]) => key.seed_value.copy_from_slice(r.take(32)?),
                ("finger_print", [33, 32]) => {
                    if r.n(1)? != 0 {
                        key.finger_print = Some(r.text()?);
                    }
                }
                _ => r.skip(code, 0)?,
            }
        }
    }
    if r.pos != data.len() || key.seed_value == [0; 32] {
        return Err(Error::InvalidKeyFile);
    }
    Ok(key)
}
