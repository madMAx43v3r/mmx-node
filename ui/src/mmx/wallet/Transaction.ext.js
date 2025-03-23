import { zlibSync, unzlibSync, strToU8, strFromU8 } from "fflate";
import { base64urlnopad } from "@scure/base";

import { Transaction } from "./Transaction";

const fields =
    "__type id version expires fee_ratio static_cost max_fee_amount note nonce network sender inputs outputs execute solutions deploy exec_result content_hash" +
    "address contract amount memo solution flags signature" +
    "method args user withdraw symbol binary init_method init_args";

const fullValues =
    "BURN CLAIM DEPLOY DEPOSIT EXECUTE MINT MUTATE OFFER REVOKE TRADE TRANSFER WITHDRAW" +
    "mmx.solution.PubKey mmx.contract.Executable mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev" +
    "mainnet";

const partValues = '00000000 null [{""}]';

const dictData = [
    ...fields.split(" ").map((value) => `"${value}":`),
    ...fullValues.split(" ").map((value) => `"${value}"`),
    ...partValues.split(" "),
].join("");

const dictionary = strToU8(dictData);

Transaction.prototype.toCompressed = function () {
    const txStr = this.toString();
    const tmpTx = Transaction.parse(txStr);

    delete tmpTx.__type;
    delete tmpTx.exec_result;
    //delete tmpTx.content_hash;

    const inputStr = tmpTx.toString();
    const inputUint8 = strToU8(inputStr);

    const level = 9;
    const options = { level, dictionary };
    const compressed = zlibSync(inputUint8, options);
    return compressed;
};

Transaction.prototype.toCompressedBase64 = function () {
    return base64urlnopad.encode(this.toCompressed());
};

Transaction.fromCompressed = function (compressed) {
    const options = { dictionary };
    const decompressed = strFromU8(unzlibSync(compressed, options));
    return Transaction.parse(decompressed);
};

Transaction.fromCompressedBase64 = function (str) {
    return Transaction.fromCompressed(base64urlnopad.decode(str));
};
