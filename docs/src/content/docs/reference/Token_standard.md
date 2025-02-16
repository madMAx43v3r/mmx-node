---
title: Token Standard
---

Every smart contract has the following hard coded fields:
- `name`: Full name (with whitespace)
- `symbol`: Maximum of 6 characters (no whitespace)
- `decimals`: How many decimals for display
- `meta_data`: Generic variable with extra info

By convention, `meta_data` is an Object with the following fields:
- `is_token`: Boolean to indiciate a standard token
- `icon_url`: Currency icon (shown next to balance)
- `logo_url`: Bigger logo image (shown on details page)
- `website_url`: Website link
- `description`: Longer text block with newlines, etc

The format to specify on-chain `WebData` resources per url is: `mmx://mmx1...`
