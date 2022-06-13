const langEN = {
    "common": {
        "yes": "Yes",
        "no": "No",

        "default": "default",

        "node": "Node",
        "wallet": "Wallet",
        "height": "Height",
        "blocks": "Blocks",
        "balance": "Balance",
        "token": "Token",
        "hash": "Hash",
        "reward": "Reward",
        "amount": "Amount",
        "address": "Address",
        "type": "Type",
        "time": "Time",
        "fee": "Fee",        
        "pending": "pending",
        
        "confirm": "Confirm",
        "deploy": "Deploy",
        "create": "Create",

        "transaction_has_been_sent": "Transaction has been sent",
        "failed_with": "Failed with",
        "deployed_as": "Deployed as",

        "link": "Link",

        "reward_address_placeholder": "{'<'}@.lower:common.default{'>'}"
    },

    "main_menu": {
        "node": "@:common.node",
        "wallet": "@:common.wallet",

        "explore": "Explore",
        "market": "Market",
        "exchange": "Exchange",
        "settings": "Settings",
        "logout": "Logout"
    },

    "node_common": {
        "vdf_speed": "VDF Speed",
        "block_reward": "Block Reward",
        "netspace": "Netspace"
    },

    "node_menu": {
        "log": "Log",
        "peers": "Peers",

        "netspace": "@:node_common.netspace",
        "blocks": "@:common.blocks",        
        "vdf_speed": "@:node_common.vdf_speed",        
        "block_reward": "@:node_common.block_reward"
    },

    "node_info": {
        "synced": "Synced",
        "supply": "Supply",
        "no_addresss": "No. Addresss",

        "height": "@:common.height",
        "netspace": "@:node_common.netspace",
        "vdf_speed": "@:node_common.vdf_speed",
        "block_reward": "@:node_common.block_reward"
    },

    "node_log": {   
        "terminal": "Terminal",
        "router": "Router",
        "farmer": "Farmer",
        "harvester": "Harvester",
        "timelord": "TimeLord",

        "wallet": "@:common.wallet",
        "node": "@:common.node"
    },

    "node_peers": {
        "ip": "IP",
        "height": "@:common.height",

        "type": "@:common.type",
        "version": "Version",
        "received": "Received",
        "send": "Send",
        "ping": "Ping",
        "duration": "Duration",
        "credits": "Credits",
        "tx_gas": "TX Gas",
        "connection": "Connection",
        "outbound": "outbound",
        "inbound": "inbound"
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

    "wallet_summary": {
        "new": "New",
        "new_wallet": "@:wallet_summary.new @:common.wallet"
    },

    "account_header": {
        "wallet": "@:common.wallet"
    },

    "account_menu": {
        
        "nfts": "NFTs",
        "contracts": "Contracts",
        "addresses": "Addresses",
        "send": "Send",
        "offer": "Offer",
        "history": "History",
        "details": "Details",

        "balance": "@:common.balance",
        "log": "@:node_menu.log"
    },

    "account_addresses": {
        "index": "#",
        "address": "@:common.address",
        "n_recv": "N(Recv)",
        "n_spend": "N(Spend)",
        "last_recv": "Last Recv",
        "last_spend": "Last Spend"
    },

    "account_send_form": {
        "amount": "@:common.amount",
        "wallet": "@:common.wallet", 

        "source_address": "Source Address",
        "destination": "Destination",
        "destination_address": "Destination Address",
        "address_input": "Address Input",
        "currency": "Currency",
        "send": "Send",
        "confirm": "@:common.confirm"
    },

    "balance_table": {
        "locked": "Locked",
        "spendable": "Spendable",
        "contract": "Contract",
        
        "token": "@:common.token",
        "balance": "@:common.balance"
    },

    "account_balance": {
        "reserved": "Reserved",

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

        "offer": "Offer",
        "receive": "Receive",
        "status": "Status",
        "actions": "Actions",
        "revoke": "Revoke",

        "accepted": "Accepted",
        "revoked": "Revoked",

        "open": "Open"
    },

    "account_offer_form": {
        "confirm": "@:common.confirm",

        "offer_amount": "Offer Amount",
        "offer_currency": "Offer Currency",
        "receive_amount": "Receive Amount",
        "receive_currency_contract": "Receive Currency Contract",
        "symbol": "Symbol",
        "offer": "Offer"

    },    

    "explore_menu": {
        "blocks": "@:common.blocks",
        "transactions": "Transactions",

        "transaction_id": "@:explore_transactions.transaction_id",
        "address": "@.lower:common.address",
        "block_height": "block height",

        "placeholder": "@.lower:explore_menu.address {'|'} @.lower:explore_menu.transaction_id  {'|'}  @.lower:explore_menu.block_height"
    },

    "explore_blocks": {
        "height": "@:common.height",
        "reward": "@:common.reward",
        "hash": "@:common.hash",

        "k": "K",
        "tx": "TX",
        
        "score": "Score",
        
        "tdiff": "T-Diff",
        "sdiff": "S-Diff"
    },

    "explore_transactions": {
        "height": "@:common.height",

        "type": "@:common.type",
        "fee": "@:common.fee",
        "n_in": "N(in)",
        "n_out": "N(out)",
        "n_op": "N(op)",
        "transaction_id": "Transaction ID"
    },

    "block_view": {
        "height": "@:common.height",
        "hash": "@:common.hash",

        "amount": "@:common.amount",
        "address": "@:common.address",
        "reward": "@:common.reward",        

        "previous": "Previous",
        "next": "Next",        
        "block": "Block",
        "no_such_block": "No such block!",

        "time": "@:common.time",
        "time_diff": "Time Diff",
        "space_diff": "Space Diff",
        "vdf_iterations": "VDF Iterations",
        "tx_base": "TX Base",
        "tx_count": "TX Count",
        "k_size": "K Size",
        "proof_score": "Proof Score",
        "plot_id": "Plot ID",
        "farmer_key": "Farmer Key",

        "inputs": "Inputs",
        "outputs": "Outputs",
        "operations": "Operations",
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
        "token": "@:common.token",      

        "transaction": "Transaction",
        "confirmed": "Confirmed",
        "expires": "Expires",
        "note": "Note",

        "sender": "Sender",
        "cost": "Cost",
        "input": "Input",
        "output": "Output"
    },

    "create_contract_menu": {
        "contract_type": "Contract Type",        
    },

    "account_contract_summary": {
        "deposit": "Deposit",
        "withdraw": "Withdraw"
    },

    "node_settings": {
        "language": "Language",
        "enable_timelord": "Enable TimeLord",
        "farmer_reward_address": "Farmer Reward Address",
        "timeLord_reward_address": "TimeLord Reward Address",
        "restart_needed": "(restart needed to apply)"
    },

    "create_locked_contract": {
        "owner_address": "Owner Address",
        "unlock_height": "Unlock at Chain Height"
    },

    "create_virtual_plot_contract": {
        "farmer_public_key": "Farmer Public Key",
        "reward_address": "Reward Address (optional, for pooling)",         
    },
    
    "account_details": {
        "copy_keys_to_plotter": "Copy keys to plotter"
    }, 

    "account_actions": {
        "reset_cache": "Reset Cache",
        "show_seed": "Show Seed"
    },

    "create_account": {
        "account_index": "Account Index",
        "account_name": "Account Name",
        "number_of_addresses": "Number of Addresses",
        "create_account": "Create Account"
    },

    "create_wallet": {
        "account_name": "@:create_account.account_name",
        "number_of_addresses": "@:create_account.number_of_addresses",

        "use_custom_seed": "Use Custom Seed",
        "seed_hash": "Seed Hash (optional, hex string, 64 chars)",
        "placeholder": "<random>",
        "create_wallet": "Create Wallet"
    },
    
    "market_menu": {
        "wallet": "@:common.wallet",
        "they_offer": "They Offer",
        "they_ask": "They Ask",
        "anything": "Anything",
        "offers": "Offers"
    },

    "market_offers": {
        "they_offer": "@:market_menu.they_offer",
        "they_ask": "@:market_menu.they_ask",
        "time": "@:common.time",
        "link": "@:common.link",
        "address": "@:common.address",

        "price": "Price",
        "accept": "Accept",
        "cancel": "Cancel",

        "accept_offer": "Accept Offer",
        "you_receive": "You Receive",
        "you_pay": "You Pay"
    },

    "login": {
        "password_label": "Password (see PASSWD file)",
        "login": "Login",
    }

}