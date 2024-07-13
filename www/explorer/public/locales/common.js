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
        "node": "@:common.node",
        "harvester": "@:common.harvester",
    },

    "harvester_settings": {
        "harvester": "@:common.harvester"
    },

    "node_log_table": {
        "time": "@:common.time",
    },

    "node_peers": {
    	"ip": "IP",
        "height": "@:common.height",
        "type": "@:common.type"
    },

    "netspace_graph": {
        "title": "@:node_common.netspace ({0})"
    },

    "vdf_speed_graph": {
        "title": "@:node_common.vdf_speed ({0})"
    },

    "block_reward_graph": {
        "title": "@:node_common.block_reward ({0})"
    },
    
    "tx_fees_graph": {
        "title": "@:node_common.tx_fees ({0})"
    },    

    "wallet_summary": {
        "new_wallet": "@:wallet_summary.new @:common.wallet"
    },

    "account_menu": {
        "balance": "@:common.balance",
        "log": "@:node_menu.log"
    },

    "account_header": {
        "wallet": "@:common.wallet"
    },    

    "account_addresses": {
        "address": "@:common.address",
    },

    "account_send_form": {
        "amount": "@:common.amount",
        "wallet": "@:common.wallet", 
        "confirm": "@:common.confirm",
        "currency": "@:common.currency"
    },

    "wallet_settings": {
        "name": "@:common.name",
        "symbol": "@:common.symbol",
        "contract": "@:common.contract"
    },

    "balance_table": {
        "token": "@:common.token",
        "balance": "@:common.balance",
        "contract": "@:common.contract"
    },

    "account_balance": {
        "balance": "@:common.balance",
        "spendable": "@:balance_table.spendable",
        "token": "@:common.token",
        "contract": "@:balance_table.contract"
    },

    "account_plots": {
        "balance": "@:common.balance",
        "address": "@:common.address"
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
        "status": "@:common.status",
        "pending": "@:common.pending",
        "expired": "@:common.expired",
        "failed": "@:common.failed"
    },

    "account_offers": {
        "any": "@:common.any",
        "height": "@:common.height",
        "address": "@:common.address",
        "time": "@:common.time",
        "deposit": "@:common.deposit",
        "price": "@:common.price"
    },

    "account_offer_form": {
        "confirm": "@:common.confirm",
    },    

    "explore_menu": {
        "blocks": "@:common.blocks",

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
        "price": "@:common.price",
        "they_offer": "@:market_menu.they_offer",
        "they_ask": "@:market_menu.they_ask",
        "time": "@:common.time",
        "link": "@:common.link",
        "address": "@:common.address",

        "accept": "@:common.accept",
        "cancel": "@:common.cancel"
    },

    "account_history_form": {
        "any": "@:common.any"
    },

    "farmer_blocks": {
        "height": "@:common.height",
        "reward": "@:common.reward",
        "score": "@:explore_blocks.score",
        "tx_fees": "@:node_common.tx_fees",
        "time": "@:common.time"
    },

    "farmer_proofs": {
        "height": "@:common.height",
        "score": "@:explore_blocks.score",
        "sdiff": "@:explore_blocks.sdiff",
        "time": "@:common.time",
        "plot_id": "@:block_view.plot_id",
        "harvester": "@:common.harvester"
    },

    "swap": {
        "volume_24h": "@:swap.volume (@:swap.24h)",
        "volume_7d": "@:swap.volume (@:swap.7d)"
    }

}