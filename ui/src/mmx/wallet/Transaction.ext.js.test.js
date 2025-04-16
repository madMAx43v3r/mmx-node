import { describe, it, assert } from "vitest";

import { Transaction } from "./Transaction";
import "./Transaction.ext";

describe(`Transaction Extensions`, () => {
    const test = (json) => {
        const tx0 = Transaction.parse(json);

        const compressed = tx0.toCompressed();
        const tx1 = Transaction.fromCompressed(compressed);

        const compressedBase64 = tx0.toCompressedBase64();
        const tx2 = Transaction.fromCompressedBase64(compressedBase64);

        //---------------------------------------------------------------
        // const origSize = tx0.toString().length;
        // const compressedSize = compressed.length;
        // const compressedBase64Size = compressedBase64.length;
        // console.log(
        //     `Original: ${origSize}; compressed: ${compressedSize}; compressedBase64: ${compressedBase64Size}; ratio: ${((origSize / compressedSize) * 100).toFixed(0)}%; ratioB64: ${((origSize / compressedBase64Size) * 100).toFixed(0)}%`
        // );
        //---------------------------------------------------------------

        assert.equal(tx0.toString(), tx1.toString(), tx2.toString());
    };
    it("compression #1", () => {
        const json =
            '{"__type":"mmx.Transaction","id":"435A65B198781DB2B174A2AA0B7F4397BC1BB7BC1B1E7913FD3AA88E3DB8B4B3","version":0,"expires":-1,"fee_ratio":1024,"static_cost":60000,"max_fee_amount":5050000,"note":"TRANSFER","nonce":11524012689813078763,"network":"mainnet","sender":"mmx1f4kxxakajacx6x48rkga3lkuzyrrmhk8kfpzpvxy0pz3vph36j2qfcs7j0","inputs":[{"address":"mmx1f4kxxakajacx6x48rkga3lkuzyrrmhk8kfpzpvxy0pz3vph36j2qfcs7j0","amount":1,"memo":"qr code tx","solution":0,"flags":0}],"outputs":[{"address":"mmx16aq5vpcmxcrh9xck0z06eqnmr87w5r2j062snjj6g7cvj0thry7q0mp3w6","amount":1,"memo":"qr code tx"}],"execute":[],"solutions":[{"__type":"mmx.solution.PubKey","version":0,"pubkey":"02C39576A35BC44A923BCDFD32EB8E947B5E44E96C6B89E83DEB76E9FEE188CAE1","signature":"F911A4B6AE4508D1979810B78D1DA50A57F1BA724EDDB981F3466FAEAFA34A302C773D471871834787816C6B819AFE616C6D03D2469609AC075D557E930849AA"}],"deploy":null,"exec_result":null,"content_hash":"6FA63C60B598D9128A779A2336D3D5D8C2D03DC7BA33725E9D344FAF0DA3B4D5"}';
        test(json);
        // Original: 998; compressed: 487; compressedBase64: 650; ratio: 205%; ratioB64: 154%
    });

    it("compression #2", () => {
        const json =
            '{"__type": "mmx.Transaction", "id": "BE81B668509D9611DBBEBAC9B21CC44250DC97278720FABFA18AEAB7F4A78A10", "version": 0, "expires": 20958, "fee_ratio": 1024, "static_cost": 50000, "max_fee_amount": 5040000, "note": "TRANSFER", "nonce": 16274384975138489954, "network": "mainnet", "sender": "mmx16aq5vpcmxcrh9xck0z06eqnmr87w5r2j062snjj6g7cvj0thry7q0mp3w6", "inputs": [{"address": "mmx16aq5vpcmxcrh9xck0z06eqnmr87w5r2j062snjj6g7cvj0thry7q0mp3w6", "contract": "mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev", "amount": "100000", "memo": null, "solution": 0, "flags": 0}], "outputs": [{"address": "mmx16aq5vpcmxcrh9xck0z06eqnmr87w5r2j062snjj6g7cvj0thry7q0mp3w6", "contract": "mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev", "amount": "100000", "memo": null}], "execute": [], "solutions": [{"__type": "mmx.solution.PubKey", "version": 0, "pubkey": "0344EE96D1B85CAC0F99B7CFA44F39EFFC590BDF51D45099D1F24AA09E5F9AD6E0", "signature": "9834FF50F306E1E911B93594F1347EEF3280EABF95E22C34F89DF679D8C21BC263DF9D84861C305C267300D749003808CE64E13BF1BFF932D0A2248C22CEE4F5"}], "deploy": null, "exec_result": null, "content_hash": "C7B2ED1BDEBC15075DC972BFE1339802625935393792C55357C3E015415CA5B4"}';
        test(json);
        // Original: 1151; compressed: 439; compressedBase64: 586; ratio: 262%; ratioB64: 196%
    });
});
