const commonLocale = {
    "common": {
        "reward_address_placeholder": "<@.lower:common.default>"
    },

    "main_menu": {
        "node": "@:common.node",
        "wallet": "@:common.wallet"
    },

    "node_menu": {
        "netspace": "@:node_common.netspace",
        "blocks": "@:common.blocks",        
        "vdf_speed": "@:node_common.vdf_speed",        
        "block_reward": "@:node_common.block_reward"
    },

    "node_info": {
        "height": "@:common.height",
        "netspace": "@:node_common.netspace",
        "vdf_speed": "@:node_common.vdf_speed",
        "block_reward": "@:node_common.block_reward",
        "block_size": "@:node_common.block_size"
    },

    "node_log": {   
        "wallet": "@:common.wallet",
        "node": "@:common.node"
    },

    "node_peers": {
    	"ip": "IP",        
        "height": "@:common.height",
        "type": "@:common.type"
    },

    "wallet_summary": {
        "new_wallet": "@:wallet_summary.new @:common.wallet"
    },

    "account_menu": {
        "balance": "@:common.balance",
        "log": "@:node_menu.log"
    },

    "account_addresses": {
        "n_recv": "N(Recv)",
        "n_spend": "N(Spend)",
        "last_recv": "Last Recv",
        "last_spend": "Last Spend"
    },    

    "account_addresses": {
        "index": "#",
        "address": "@:common.address"
    },

    "account_send_form": {
        "amount": "@:common.amount",
        "wallet": "@:common.wallet", 
        "confirm": "@:common.confirm"
    },

    "balance_table": {
        "token": "@:common.token",
        "balance": "@:common.balance"
    },

    "account_balance": {
        "balance": "@:common.balance",
        "spendable": "@:balance_table.spendable",
        "token": "@:common.token",
        "contract": "@:balance_table.contract"
    },

    "account_history": {
        "height": "@:common.height",
        "type": "@:common.type",
        "amount": "@:common.amount",
        "token": "@:common.token",
        "address": "@:common.address",
        "time": "@:common.time",
        "link": "@:common.link"
    },    

    "account_tx_history": {
        "height": "@:common.height",
        "confirmed": "@:transaction_view.confirmed",
        "transaction_id": "@:explore_transactions.transaction_id",
        "time": "@:common.time",
        "pending": "@:common.pending"
    },

    "account_offers": {
        "height": "@:common.height",
        "address": "@:common.address",
        "time": "@:common.time",
    },

    "account_offer_form": {
        "confirm": "@:common.confirm",
    },    

    "explore_menu": {
        "blocks": "@:common.blocks",
        "transactions": "Transactions",

        "transaction_id": "@:explore_transactions.transaction_id",
        "address": "@.lower:common.address",

        "placeholder": "@.lower:explore_menu.address | @.lower:explore_menu.transaction_id | @.lower:explore_menu.block_height"
    },

    "explore_blocks": {
        "height": "@:common.height",
        "reward": "@:common.reward",
        "hash": "@:common.hash"
    },

    "explore_transactions": {
        "height": "@:common.height",

        "type": "@:common.type",
        "fee": "@:common.fee"
    },

    "block_view": {
        "height": "@:common.height",
        "hash": "@:common.hash",

        "amount": "@:common.amount",
        "address": "@:common.address",
        "reward": "@:common.reward",        

        "time": "@:common.time",

        "transaction_id": "@:explore_transactions.transaction_id"     
    },

    "address_history_table": {
        "height": "@:common.height",
        "type": "@:common.type",
        "amount": "@:common.amount",
        "token": "@:common.token",
        "address": "@:common.address",
        "time": "@:common.time",
        "link": "@:common.link"
    },

    "transaction_view": {

        "height": "@:common.height",
        "time": "@:common.time",
        "address": "@:common.address",
        "fee": "@:common.fee",
        "amount": "@:common.amount",
        "token": "@:common.token"
    },

    "market_menu": {
        "wallet": "@:common.wallet"
    },

    "market_offers": {
        "they_offer": "@:market_menu.they_offer",
        "they_ask": "@:market_menu.they_ask",
        "time": "@:common.time",
        "link": "@:common.link",
        "address": "@:common.address"
    }

}