import { Execute } from "./common/Operation";
import { Transaction } from "./Transaction";

export class Wallet {
    static async getSendTxAsync(ecdsaWallet, amount, dst_addr, currency, options) {
        const tx = new Transaction();
        tx.note = "TRANSFER";
        tx.add_output(currency, dst_addr, amount, options.memo);
        await ecdsaWallet.completeAsync(tx, options);
        return tx;
    }

    static async getSendManyTxAsync(ecdsaWallet, payments, currency, options) {
        const tx = new Transaction();
        tx.note = "TRANSFER";
        payments.forEach((payment) => {
            tx.add_output(currency, payment.address, payment.amount, options.memo);
        });
        await ecdsaWallet.completeAsync(tx, options);
        return tx;
    }

    static async getExecuteTxAsync(ecdsaWallet, address, method, args, user, options) {
        const op = {
            address,
            method,
            args,
            user: user !== null ? await ecdsaWallet.getAddressAsync(user) : options.user,
        };

        const tx = new Transaction();
        tx.note = "EXECUTE";
        tx.execute.push(new Execute(op));
        await ecdsaWallet.completeAsync(tx, options);
        return tx;
    }
}
