use crate::{sha256, Error, Result};
/// MMX uses BIP39's English words, a reversed seed byte order, and accepts
/// both historic little-endian and current big-endian checksum variants.
pub fn seed_to_words(seed: &[u8; 32]) -> String {
    let words: Vec<_> = include_str!("english.txt").lines().collect();
    let mut big = *seed;
    big.reverse();
    let checksum = sha256(big)[0];
    (0..24)
        .map(|i| {
            let mut index = 0usize;
            for j in 0..11 {
                let bit = i * 11 + j;
                index = (index << 1)
                    | if bit < 256 {
                        ((big[bit / 8] >> (7 - bit % 8)) & 1) as usize
                    } else {
                        ((checksum >> (7 - (bit - 256))) & 1) as usize
                    };
            }
            words[index]
        })
        .collect::<Vec<_>>()
        .join(" ")
}
pub fn words_to_seed(phrase: &str) -> Result<[u8; 32]> {
    let words: Vec<_> = include_str!("english.txt").lines().collect();
    let input: Vec<_> = phrase.split_whitespace().collect();
    if input.len() != 24 {
        return Err(Error::InvalidMnemonic);
    }
    let mut big = [0u8; 32];
    let mut checksum = 0u8;
    for (i, word) in input.iter().enumerate() {
        let index = words
            .iter()
            .position(|w| w == word)
            .ok_or(Error::InvalidMnemonic)?;
        for j in 0..11 {
            let bit = i * 11 + j;
            let v = ((index >> (10 - j)) & 1) as u8;
            if bit < 256 {
                big[bit / 8] |= v << (7 - bit % 8);
            } else {
                checksum = (checksum << 1) | v;
            }
        }
    }
    let current = sha256(big)[0];
    big.reverse();
    if current != checksum && sha256(big)[0] != checksum {
        return Err(Error::InvalidMnemonic);
    }
    Ok(big)
}
