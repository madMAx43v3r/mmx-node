import { Deposit, Execute } from "./common/Operation";
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

    static async getDepositTxAsync(ecdsaWallet, address, method, args, amount, currency, options) {
        const op = {
            address,
            method,
            args,
            amount,
            currency,
            user: options.user,
        };

        const tx = new Transaction();
        tx.note = "DEPOSIT";
        tx.execute.push(new Deposit(op));
        await ecdsaWallet.completeAsync(tx, options);
        return tx;
    }
}

/*

    +send(index, amount, dst_addr, currency, options)
    +send_many(index, amounts, currency, options)
    send_from(index, amount, dst_addr, src_addr, currency, options)
    deploy(index, contract, options)
    +execute(index, address, method, args, user, options)
    deposit(index, address, method, args, amount, currency, options)
    make_offer(index, owner, bid_amount, bid_currency, ask_amount, ask_currency, options)
    offer_trade(index, address, amount, dst_addr, price, options)
    accept_offer(index, address, dst_addr, price, options)
    offer_withdraw(index, address, options)
    cancel_offer(index, address, options)
    swap_trade(index, address, amount, currency, min_trade, num_iter, options)
    swap_add_liquid(index, address, amount, pool_idx, options)
    swap_rem_liquid(index, address, amount, options)
    plotnft_exec(address, method, args, options)
    plotnft_create(index, name, owner, options)

*/
