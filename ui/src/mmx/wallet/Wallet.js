import { Transaction } from "./Transaction";

class Wallet {
    static async getSendTxAsync(ecdsaWallet, amount, dst_addr, currency, options) {
        const tx = new Transaction();
        tx.note = "TRANSFER";
        tx.add_output(currency, dst_addr, amount, options.memo);
        await ecdsaWallet.completeAsync(tx, options);
        return tx;
    }

    static async getSendManyTxAsync(ecdsaWallet, payments, dst_addr, currency, options) {
        const tx = new Transaction();
        tx.note = "TRANSFER";
        payments.forEach((payment) => {
            tx.add_output(currency, payment.address, payment.amount, options.memo);
        });
        await ecdsaWallet.completeAsync(tx, options);
        return tx;
    }
}
export { Wallet };
