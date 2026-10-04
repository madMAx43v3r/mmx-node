use crate::{atomic, hex_bytes, sha256, Address, Error, Result, Wallet};
use k256::ecdsa::{
    signature::hazmat::{PrehashSigner, PrehashVerifier},
    Signature, VerifyingKey,
};
use rand::RngCore;
use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, BTreeSet};

/// Only consensus parameters needed by simple transfers. Other RPC chain
/// parameters may be present and are ignored by serde.
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(default)]
pub struct ChainParams {
    pub network: String,
    pub decimals: u32,
    pub min_txfee: u64,
    pub min_txfee_io: u64,
    pub min_txfee_sign: u64,
    pub min_txfee_memo: u64,
}
impl Default for ChainParams {
    fn default() -> Self {
        Self {
            network: "mainnet".into(),
            decimals: 6,
            min_txfee: 100,
            min_txfee_io: 100,
            min_txfee_sign: 1000,
            min_txfee_memo: 50,
        }
    }
}
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Input {
    pub address: Address,
    pub contract: Address,
    #[serde(with = "atomic")]
    pub amount: u128,
    pub memo: Option<String>,
    pub solution: u16,
    pub flags: u8,
}
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Output {
    pub address: Address,
    pub contract: Address,
    #[serde(with = "atomic")]
    pub amount: u128,
    pub memo: Option<String>,
}
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Solution {
    #[serde(rename = "__type")]
    pub type_name: String,
    pub version: u32,
    #[serde(with = "hex_bytes")]
    pub pubkey: [u8; 33],
    #[serde(with = "hex_bytes")]
    pub signature: [u8; 64],
}
/// Signed simple-transfer transaction. Contract calls/deployments are outside
/// this wallet API; nonempty execute/deploy/exec_result are rejected.
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Transaction {
    #[serde(rename = "__type")]
    pub type_name: String,
    #[serde(with = "hex_bytes")]
    pub id: [u8; 32],
    pub version: u32,
    pub expires: u32,
    pub fee_ratio: u32,
    pub static_cost: u32,
    pub max_fee_amount: u32,
    pub note: String,
    pub nonce: u64,
    pub network: String,
    pub sender: Option<Address>,
    pub inputs: Vec<Input>,
    pub outputs: Vec<Output>,
    pub execute: Vec<serde_json::Value>,
    pub solutions: Vec<Solution>,
    pub deploy: Option<serde_json::Value>,
    pub exec_result: Option<serde_json::Value>,
    #[serde(with = "hex_bytes")]
    pub content_hash: [u8; 32],
}
#[derive(Clone, Debug)]
pub struct TransferOptions {
    pub fee_ratio: u32,
    pub expire_delta: u32,
    /// Set for reproducible offline signing. None uses the OS RNG.
    pub nonce: Option<u64>,
    pub gas_limit: u64,
}
impl Default for TransferOptions {
    fn default() -> Self {
        Self {
            fee_ratio: 1024,
            expire_delta: 100,
            nonce: None,
            gas_limit: 5000000,
        }
    }
}

// Consensus hash serialization mirrors include/mmx/write_bytes.h: integer
// widths are promoted to u64, byte/string lengths and vector counts are u64.
struct HashWriter(Vec<u8>);
impl HashWriter {
    fn raw(&mut self, b: &[u8]) {
        self.0.extend(b);
    }
    fn n(&mut self, n: u64) {
        self.raw(&n.to_le_bytes());
    }
    fn bytes(&mut self, b: &[u8]) {
        self.raw(b"bytes<>");
        self.n(b.len() as u64);
        self.raw(b);
    }
    fn text(&mut self, s: &str) {
        self.raw(b"string<>");
        self.n(s.len() as u64);
        self.raw(s.as_bytes());
    }
    fn field(&mut self, s: &str) {
        self.raw(b"field<>");
        self.text(s);
    }
    fn number(&mut self, name: &str, n: u64) {
        self.field(name);
        self.n(n);
    }
    fn vector(&mut self, count: usize) {
        self.raw(b"vector<>");
        self.n(count as u64);
    }
    fn memo(&mut self, memo: &Option<String>) {
        self.raw(b"optional<>");
        self.0.push(u8::from(memo.is_some()));
        if let Some(s) = memo {
            self.text(s);
        }
    }
    fn io(&mut self, address: Address, currency: Address, amount: u128, memo: &Option<String>) {
        self.bytes(&address.0);
        self.bytes(&currency.0);
        self.raw(&amount.to_le_bytes());
        self.memo(memo);
    }
}
impl Solution {
    pub fn calc_hash(&self) -> [u8; 32] {
        let mut w = HashWriter(Vec::new());
        w.n(0xe47af6fcacfcefa5);
        w.number("version", self.version as u64);
        w.field("pubkey");
        w.bytes(&self.pubkey);
        w.field("signature");
        w.bytes(&self.signature);
        sha256(w.0)
    }
}
impl Transaction {
    pub fn hash_serialize(&self, full: bool) -> Result<Vec<u8>> {
        if self.version != 1
            || !self.execute.is_empty()
            || self.deploy.is_some()
            || self.exec_result.is_some()
            || self.note != "TRANSFER"
        {
            return Err(Error::InvalidTransaction(
                "only hardfork2 simple transfers are supported",
            ));
        }
        let mut w = HashWriter(Vec::new());
        w.n(0xce0462acdceaa5bc);
        w.number("version", self.version as u64);
        w.number("expires", self.expires as u64);
        w.number("fee_ratio", self.fee_ratio as u64);
        w.number("max_fee_amount", self.max_fee_amount as u64);
        w.number("note", 858544509);
        w.number("nonce", self.nonce);
        w.field("network");
        w.text(&self.network);
        w.field("sender");
        w.raw(b"optional<>");
        w.0.push(u8::from(self.sender.is_some()));
        if let Some(a) = self.sender {
            w.bytes(&a.0);
        }
        w.field("inputs");
        w.vector(self.inputs.len());
        for i in &self.inputs {
            w.raw(b"txin_t<>");
            w.io(i.address, i.contract, i.amount, &i.memo);
            if full {
                w.n(i.solution as u64);
                w.n(i.flags as u64);
            }
        }
        w.field("outputs");
        w.vector(self.outputs.len());
        for o in &self.outputs {
            w.raw(b"txout_t<>");
            w.io(o.address, o.contract, o.amount, &o.memo);
        }
        w.number("execute", 0);
        w.field("deploy");
        w.bytes(&[0; 32]);
        if full {
            w.number("static_cost", self.static_cost as u64);
            w.number("solutions", self.solutions.len() as u64);
            for s in &self.solutions {
                w.bytes(&s.calc_hash());
            }
            w.field("exec_result");
            w.bytes(&[0; 32]);
        }
        Ok(w.0)
    }
    pub fn calc_hash(&self, full: bool) -> Result<[u8; 32]> {
        Ok(sha256(self.hash_serialize(full)?))
    }
    pub fn id_hex(&self) -> String {
        hex::encode(self.id)
    }
    pub fn calc_cost(&self, params: &ChainParams) -> Result<u32> {
        let mut cost = params.min_txfee as u128;
        for memo in self
            .inputs
            .iter()
            .map(|i| &i.memo)
            .chain(self.outputs.iter().map(|o| &o.memo))
        {
            cost += params.min_txfee_io as u128;
            if let Some(memo) = memo {
                cost += (memo.len().div_ceil(32).max(1) as u128) * params.min_txfee_memo as u128;
            }
        }
        cost += self.solutions.len() as u128 * params.min_txfee_sign as u128;
        cost.try_into()
            .map_err(|_| Error::InvalidTransaction("static cost overflow"))
    }
    /// Check both hashes, cost, solution references and ECDSA signatures before
    /// a saved transfer is offered to an RPC for validation or broadcast.
    pub fn verify(&self, params: &ChainParams) -> Result<()> {
        let bad = |s| Error::InvalidTransaction(s);
        if self.type_name != "mmx.Transaction"
            || self.version != 1
            || self.nonce == 0
            || self.fee_ratio < 1024
            || self.network != params.network
            || self.inputs.is_empty()
            || self.outputs.is_empty()
            || self.solutions.len() > 65535
        {
            return Err(bad("invalid transfer fields"));
        }
        if self
            .inputs
            .iter()
            .any(|i| i.flags != 0 || i.amount == 0 || i.memo.as_ref().is_some_and(|m| m.len() > 64))
            || self
                .outputs
                .iter()
                .any(|o| o.amount == 0 || o.memo.as_ref().is_some_and(|m| m.len() > 64))
        {
            return Err(bad("invalid input/output"));
        }
        if self.id != self.calc_hash(false)?
            || self.content_hash != self.calc_hash(true)?
            || self.static_cost != self.calc_cost(params)?
        {
            return Err(bad("hash or cost mismatch"));
        }
        let mut addresses = Vec::new();
        let mut hashes = BTreeSet::new();
        for sol in &self.solutions {
            if sol.type_name != "mmx.solution.PubKey" || sol.version != 0 {
                return Err(bad("unsupported solution version"));
            }
            let key = VerifyingKey::from_sec1_bytes(&sol.pubkey)
                .map_err(|_| bad("invalid public key"))?;
            let sig =
                Signature::from_slice(&sol.signature).map_err(|_| bad("invalid signature"))?;
            if sig.normalize_s().is_some() {
                return Err(bad("noncanonical signature"));
            }
            key.verify_prehash(&self.id, &sig)
                .map_err(|_| bad("signature verification failed"))?;
            addresses.push(Address(sha256(sol.pubkey)));
            if !hashes.insert(sol.calc_hash()) {
                return Err(bad("duplicate solution"));
            }
        }
        let mut used = BTreeSet::new();
        if let Some(sender) = self.sender {
            if addresses.first() != Some(&sender) {
                return Err(bad("sender signature missing"));
            }
            used.insert(0);
        }
        for i in &self.inputs {
            if addresses.get(i.solution as usize) != Some(&i.address) {
                return Err(bad("input signature missing"));
            }
            used.insert(i.solution as usize);
        }
        if used.len() != addresses.len() {
            return Err(bad("unused solution"));
        }
        Ok(())
    }
}
impl Wallet {
    pub fn transfer(
        &self,
        balances: &BTreeMap<(Address, Address), u128>,
        params: &ChainParams,
        height: u32,
        output: Output,
        options: &TransferOptions,
    ) -> Result<Transaction> {
        if output.amount == 0
            || output.address == Address::default()
            || output.memo.as_ref().is_some_and(|m| m.len() > 64)
            || options.fee_ratio < 1024
            || options.expire_delta == 0
        {
            return Err(Error::InvalidArgument("invalid transfer options".into()));
        }
        let expires = height
            .checked_add(options.expire_delta)
            .ok_or_else(|| Error::InvalidArgument("transaction expiry overflow".into()))?;
        let mut tx = Transaction {
            type_name: "mmx.Transaction".into(),
            id: [0; 32],
            version: 1,
            expires,
            fee_ratio: options.fee_ratio,
            static_cost: 0,
            max_fee_amount: 0,
            note: "TRANSFER".into(),
            nonce: options
                .nonce
                .unwrap_or_else(|| rand::rngs::OsRng.next_u64().max(1)),
            network: params.network.clone(),
            sender: None,
            inputs: Vec::new(),
            outputs: vec![output],
            execute: Vec::new(),
            solutions: Vec::new(),
            deploy: None,
            exec_result: None,
            content_hash: [0; 32],
        };
        let mut candidates: Vec<_> = balances
            .iter()
            .filter(|((a, c), _)| *c == tx.outputs[0].contract && self.addresses.contains(a))
            .collect();
        candidates.sort_by(|a, b| b.1.cmp(a.1));
        let mut left = tx.outputs[0].amount;
        let mut spent = BTreeMap::new();
        for ((address, currency), balance) in candidates {
            if left == 0 {
                break;
            }
            let amount = left.min(*balance);
            if amount == 0 {
                continue;
            }
            left -= amount;
            tx.inputs.push(Input {
                address: *address,
                contract: *currency,
                amount,
                memo: None,
                solution: u16::MAX,
                flags: 0,
            });
            spent.insert((*address, *currency), amount);
        }
        if left != 0 {
            return Err(Error::InsufficientFunds("not enough funds"));
        }
        let cost = tx.calc_cost(params)? as u128;
        let static_fee = cost * options.fee_ratio as u128 / 1024;
        tx.max_fee_amount = ((cost + options.gas_limit as u128) * options.fee_ratio as u128 / 1024)
            .try_into()
            .map_err(|_| Error::InvalidArgument("maximum fee exceeds 32 bits".into()))?;
        let mut sender = None;
        let mut max_amount = 0;
        for ((a, c), balance) in balances {
            if *c == Address::default() && self.addresses.contains(a) {
                let balance = balance.saturating_sub(*spent.get(&(*a, *c)).unwrap_or(&0));
                if balance > max_amount {
                    max_amount = balance;
                    sender = Some(*a);
                }
            }
        }
        if sender.is_none() || max_amount < static_fee {
            return Err(Error::InsufficientFunds("insufficient funds for tx fee"));
        }
        tx.sender = sender;
        tx.id = tx.calc_hash(false)?;
        let mut owners = vec![sender.unwrap()];
        for i in &mut tx.inputs {
            let index = if let Some(index) = owners.iter().position(|a| *a == i.address) {
                index
            } else {
                owners.push(i.address);
                owners.len() - 1
            };
            i.solution = index as u16;
        }
        for owner in owners {
            let i = self
                .addresses
                .iter()
                .position(|a| *a == owner)
                .ok_or(Error::InvalidTransaction("unknown signing address"))?;
            let key = &self.keys[i];
            let sig: Signature = key
                .sign_prehash(&tx.id)
                .map_err(|_| Error::InvalidTransaction("signing failed"))?;
            tx.solutions.push(Solution {
                type_name: "mmx.solution.PubKey".into(),
                version: 0,
                pubkey: key
                    .verifying_key()
                    .to_encoded_point(true)
                    .as_bytes()
                    .try_into()
                    .unwrap(),
                signature: sig.to_bytes().into(),
            });
        }
        tx.static_cost = tx.calc_cost(params)?;
        tx.content_hash = tx.calc_hash(true)?;
        tx.verify(params)?;
        Ok(tx)
    }
}
