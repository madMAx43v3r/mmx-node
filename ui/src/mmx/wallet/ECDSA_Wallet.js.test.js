import { describe, it, assert, expect } from "vitest";

import { hexToBytes } from "@noble/hashes/utils";

import { mnemonicToSeed } from "./mnemonic";
import { ECDSA_Wallet } from "./ECDSA_Wallet";

import "./utils/Uint8ArrayUtils";
import { sign, signAsync } from "./common/ECDSAUtils";

describe("ECDSA_Wallet", async () => {
    const mnemonic = import.meta.env.VITE_TEST_MNEMONIC;

    const seed = mnemonicToSeed(mnemonic);

    const ecdsaWallet = new ECDSA_Wallet(seed);
    const farmerKey = await ecdsaWallet.getFarmerKeyAsync();

    const fingerPrint = await ecdsaWallet.getFingerPrintAsync();

    const address = await ecdsaWallet.getAddressAsync(0);
    const address1 = await ecdsaWallet.getAddressAsync(1);

    const ecdsaWallet2 = new ECDSA_Wallet(seed, "passphrase");
    const fingerPrintWithPassphrase = await ecdsaWallet2.getFingerPrintAsync();
    const addressWithPassphrase = await ecdsaWallet2.getAddressAsync(0);
    const addressWithPassphrase1 = await ecdsaWallet2.getAddressAsync(1);

    it("invalid seed type", () => {
        expect(() => new ECDSA_Wallet(Symbol("invalid seed type"))).toThrowError();
    });

    it("createFarmerKey", () => {
        assert.equal(farmerKey.toHex(), "022C514E47B3C5015D4118F4FF7E38041431358D4B16CB9654C1ABD28D8E1FD8DC");
    });

    it("getFingerPrint", () => {
        assert.equal(fingerPrint, 173071474);
    });

    it("getFingerPrint with passphrase", () => {
        assert.equal(fingerPrintWithPassphrase, 1974242666);
    });

    it("getAddress #1", () => {
        assert.equal(address, "mmx16aq5vpcmxcrh9xck0z06eqnmr87w5r2j062snjj6g7cvj0thry7q0mp3w6");
    });

    it("getAddress with passphrase #1", () => {
        assert.equal(addressWithPassphrase, "mmx1ygwhdcutgezfeke0699vmwdvaypnjf8kmrw4r5kxwe3cwq5tnwdsv6cyga");
    });

    it("getAddress #2", () => {
        assert.equal(address1, "mmx1fsg8a3acsg0md9h39gldrdzpgwmzg887zsa2jdagydk5p5fggf7s2yk5hr");
    });

    it("getAddress with passphrase #2", () => {
        assert.equal(addressWithPassphrase1, "mmx12gfh8lswlcccefwnev6fp66h6wu3myjaqqw9gspjy9cqdcnz89dslkeym4");
    });

    it("getKeys", async () => {
        const { privKey, pubKey } = await ecdsaWallet.getKeysAsync(0);
        assert.equal(pubKey.toHex(), "0344EE96D1B85CAC0F99B7CFA44F39EFFC590BDF51D45099D1F24AA09E5F9AD6E0");
        assert.equal(privKey.toHex(), "8E469EDD0D8349C59A4255F86C27B5854A071472FCDD94398E86FDB2D0CCFC9B");
    });

    it("sign #1", async () => {
        const { privKey, pubKey } = await ecdsaWallet.getKeysAsync(0);
        const msg = "65C64268B471CAF511174B7E7C3046E408EA52AB5411365488A110FC860B7ECB";

        const signature = await sign(privKey, hexToBytes(msg));
        assert.equal(
            signature.toHex(),
            "3D2D75E0DAA39933578855552D9629DB6A15FAE8C5539CC5DCE0F031349621433A78311E016044D8B4E98D775D7EB2947B977A076E3BB6058FC856CDE73EA0EB"
        );
    });

    it("sign #2", () => {
        const privKey = "3EA8B52A5E6E5730D802D2E4589575CA3FDEFF17DFBDFAF28145DADE4AB57621";
        const msg = "E7FCE58848929A0D931E5981CA7FA1DDEB3C9D32D14AC38C72C28C301EAAD990";

        const signature = sign(hexToBytes(privKey), hexToBytes(msg));
        assert.equal(
            signature.toHex(),
            "024F512B1F7149662F2D7B1901A2B1A392971091263A40E6DFE415314322EDD321CFE68AA81CDAAA854EA15F5BB9891F38A37F6CDADEFA6153F8613F7B133415"
        );
    });

    it("sign #2 async", async () => {
        const privKey = "3EA8B52A5E6E5730D802D2E4589575CA3FDEFF17DFBDFAF28145DADE4AB57621";
        const msg = "E7FCE58848929A0D931E5981CA7FA1DDEB3C9D32D14AC38C72C28C301EAAD990";

        const signature = await signAsync(hexToBytes(privKey), hexToBytes(msg));
        assert.equal(
            signature.toHex(),
            "024F512B1F7149662F2D7B1901A2B1A392971091263A40E6DFE415314322EDD321CFE68AA81CDAAA854EA15F5BB9891F38A37F6CDADEFA6153F8613F7B133415"
        );
    });
});
