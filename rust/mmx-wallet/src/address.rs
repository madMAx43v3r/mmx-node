use crate::{Error, Result};
use serde::{Deserialize, Deserializer, Serialize, Serializer};
use std::{fmt, str::FromStr};

/// MMX address bytes are little endian integers when encoded as Bech32m.
#[derive(Clone, Copy, Default, PartialEq, Eq, PartialOrd, Ord, Hash, Debug)]
pub struct Address(pub [u8; 32]);
const CHARSET: &[u8; 32] = b"qpzry9x8gf2tvdw0s3jn54khce6mua7l";
fn polymod(values: impl IntoIterator<Item = u8>) -> u32 {
    let mut chk = 1u32;
    for v in values {
        let top = chk >> 25;
        chk = ((chk & 0x1ffffff) << 5) ^ u32::from(v);
        for (i, g) in [0x3b6a57b2, 0x26508e6d, 0x1ea119fa, 0x3d4233dd, 0x2a1462b3]
            .iter()
            .enumerate()
        {
            if (top >> i) & 1 != 0 {
                chk ^= g;
            }
        }
    }
    chk
}
fn prefix() -> Vec<u8> {
    b"mmx"
        .iter()
        .map(|b| b >> 5)
        .chain([0])
        .chain(b"mmx".iter().map(|b| b & 31))
        .collect()
}
impl fmt::Display for Address {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut data = Vec::with_capacity(58);
        for group in 0..52 {
            let mut value = 0;
            for j in 0..5 {
                let bit = group * 5 + j;
                value = (value << 1)
                    | if bit < 256 {
                        (self.0[31 - bit / 8] >> (7 - bit % 8)) & 1
                    } else {
                        0
                    };
            }
            data.push(value);
        }
        let checksum = polymod(
            prefix()
                .into_iter()
                .chain(data.iter().copied())
                .chain([0; 6]),
        ) ^ 0x2bc830a3;
        for i in (0..6).rev() {
            data.push(((checksum >> (5 * i)) & 31) as u8);
        }
        write!(
            f,
            "mmx1{}",
            data.iter()
                .map(|v| CHARSET[*v as usize] as char)
                .collect::<String>()
        )
    }
}
impl FromStr for Address {
    type Err = Error;
    fn from_str(s: &str) -> Result<Self> {
        let bad = || Error::InvalidArgument("invalid MMX address".into());
        if s == "MMX" {
            return Ok(Self::default());
        }
        if s.len() != 62
            || (s.bytes().any(|b| b.is_ascii_lowercase())
                && s.bytes().any(|b| b.is_ascii_uppercase()))
        {
            return Err(bad());
        }
        let s = s.to_ascii_lowercase();
        if !s.starts_with("mmx1") {
            return Err(bad());
        }
        let data: Vec<u8> = s[4..]
            .bytes()
            .map(|b| {
                CHARSET
                    .iter()
                    .position(|c| *c == b)
                    .map(|i| i as u8)
                    .ok_or_else(bad)
            })
            .collect::<Result<_>>()?;
        if polymod(prefix().into_iter().chain(data.iter().copied())) != 0x2bc830a3
            || data[51] & 15 != 0
        {
            return Err(bad());
        }
        let mut bytes = [0u8; 32];
        for bit in 0..256 {
            bytes[31 - bit / 8] |= ((data[bit / 5] >> (4 - bit % 5)) & 1) << (7 - bit % 8);
        }
        Ok(Self(bytes))
    }
}
impl Serialize for Address {
    fn serialize<S: Serializer>(&self, s: S) -> std::result::Result<S::Ok, S::Error> {
        s.serialize_str(&self.to_string())
    }
}
impl<'de> Deserialize<'de> for Address {
    fn deserialize<D: Deserializer<'de>>(d: D) -> std::result::Result<Self, D::Error> {
        String::deserialize(d)?
            .parse()
            .map_err(serde::de::Error::custom)
    }
}
