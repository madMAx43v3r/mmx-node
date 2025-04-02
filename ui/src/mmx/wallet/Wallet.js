import { Executable } from "./common/Contract";
import { Deposit, Execute } from "./common/Operation";
import { uint128 } from "./common/uint128";
import { Transaction } from "./Transaction";
import { getChainParamsAsync } from "./utils/getChainParamsAsync";

export class Wallet {
    static async getSendTxAsync(ecdsaWallet, amount, dst_addr, currency, options) {
        if (amount == 0) {
            throw new Error("amount cannot be zero");
        }
        if (dst_addr == "") {
            throw new Error("dst_addr cannot be zero");
        }

        const tx = new Transaction();
        tx.note = "TRANSFER";
        tx.add_output(currency, dst_addr, amount, options.memo);
        await ecdsaWallet.completeAsync(tx, options);
        return tx;
    }

    static async getSendManyTxAsync(ecdsaWallet, amounts, currency, options) {
        const tx = new Transaction();
        tx.note = "TRANSFER";
        amounts.forEach((payment) => {
            if (payment.amount == 0) {
                throw new Error("amount cannot be zero");
            }
            if (payment.address == "") {
                throw new Error("dst_addr cannot be zero");
            }
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

    static async getDeployTxAsync(ecdsaWallet, contract, options) {
        const tx = new Transaction();
        tx.note = "DEPLOY";
        tx.deploy = new Executable(contract);
        await ecdsaWallet.completeAsync(tx, options);
        return tx;
    }

    static async getMakeOfferTxAsync(ecdsaWallet, bid_amount, bid_currency, ask_amount, ask_currency, options) {
        if (bid_amount == 0 || ask_amount == 0) {
            throw new Error("amount cannot be zero");
        }

        const inv_price = (BigInt(bid_amount) << 64n) / BigInt(ask_amount);
        if (inv_price >> 128n) {
            throw new Error("price out of range");
        }

        const owner = await ecdsaWallet.getAddressAsync(0);

        const chainParams = await getChainParamsAsync(options.network);

        const offer = {
            binary: chainParams.offer_binary,
            init_method: "init",
            init_args: [owner, bid_currency, ask_currency, new uint128(inv_price).toHex(), null],
        };

        const tx = new Transaction();
        tx.note = "OFFER";
        tx.deploy = new Executable(offer);

        const deposit = [[bid_currency, bid_amount]];
        await ecdsaWallet.completeAsync(tx, options, deposit);
        return tx;
    }

    static async getOfferTradeTxAsync(ecdsaWallet, address, amount, ask_currency, price, options) {
        const options_ = { ...options };
        options_.note = "TRADE";

        const args = [await ecdsaWallet.getAddressAsync(0), new uint128(price).toHex()];

        return await Wallet.getDepositTxAsync(ecdsaWallet, address, "trade", args, amount, ask_currency, options_);
    }

    static async getAcceptOfferTxAsync(ecdsaWallet, offer, price, options) {
        const args = [await ecdsaWallet.getAddressAsync(0), new uint128(price).toHex()];

        const options_ = { ...options };
        options_.note = "TRADE";
        return this.getDepositTxAsync(
            ecdsaWallet,
            offer.address,
            "accept",
            args,
            offer.ask_amount,
            offer.ask_currency,
            options_
        );
    }

    static async getCancelOfferTxAsync(ecdsaWallet, address, offer_owner, options) {
        const options_ = { ...options };
        options_.user = offer_owner;

        return this.getExecuteTxAsync(ecdsaWallet, address, "cancel", [], null, options_);
    }
}

/*
    +send(index, amount, dst_addr, currency, options)
    +send_many(index, amounts, currency, options)
    send_from(index, amount, dst_addr, src_addr, currency, options)
    +deploy(index, contract, options)
    +execute(index, address, method, args, user, options)
    +deposit(index, address, method, args, amount, currency, options)
    +make_offer(index, owner, bid_amount, bid_currency, ask_amount, ask_currency, options)
    +offer_trade(index, address, amount, dst_addr, price, options)
    +accept_offer(index, address, dst_addr, price, options)
    offer_withdraw(index, address, options)
    +cancel_offer(index, address, options)
    swap_trade(index, address, amount, currency, min_trade, num_iter, options)
    swap_add_liquid(index, address, amount, pool_idx, options)
    swap_rem_liquid(index, address, amount, options)
    plotnft_exec(address, method, args, options)
    plotnft_create(index, name, owner, options)

*/
