//! A reproducible offline transfer using a PUBLIC test seed and mock balances.
//! This is an API example, not a funded wallet.
use mmx_wallet::{Address, ChainParams, Output, TransferOptions, Wallet};
use std::collections::BTreeMap;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let seed = std::array::from_fn(|i| (i + 1) as u8);
    let wallet = Wallet::derive(&seed, "test 世界 🔑", 7, 3)?;
    let params = ChainParams::default();
    let balances = BTreeMap::from([
        ((wallet.addresses[0], Address::default()), 9_000_000),
        ((wallet.addresses[1], Address::default()), 12_000_000),
    ]);
    let tx = wallet.transfer(
        &balances,
        &params,
        5_050_000,
        Output {
            address: wallet.addresses[2],
            contract: Address::default(),
            amount: 15_000_000,
            memo: Some("memo 世界".into()),
        },
        &TransferOptions {
            nonce: Some(123456789),
            fee_ratio: 1536,
            expire_delta: 123,
            ..Default::default()
        },
    )?;
    tx.verify(&params)?;
    println!("{}", serde_json::to_string(&tx)?);
    Ok(())
}
