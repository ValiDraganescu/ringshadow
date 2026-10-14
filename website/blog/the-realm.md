# The realm: shared buildings, a tax and a shop

> Ringshadow's buildings now belong to the realm: every pilot uses them, the Citadel takes 10% of every unload, and its shop sells blueprints and supplies.

Source: https://ringshadow.com/blog/the-realm

14 October 2026 · M8 · The realm under siege

I played the new mode to level 10 and came away with a list. First on it were the buildings: a world with many pilots can't give each of them a Citadel, so the buildings had to belong to everyone.

![A Prospector unloading at the Citadel, the label reading +24 ORE, 3 TO THE REALM](https://ringshadow.com/blog/media/the-realm/01-tax.webp)

*27 ore unloaded: 24 to the pilot, 3 to the realm.*

## Buildings belong to the realm

Every building in a world is now the realm's. Any pilot unloads at any Citadel or Hab Dome, respawns at it, lands at its pads and, soon, is defended by its guns. The builder still gets the credit: the game keeps who built what, under a callsign of the form "Pilot XXXX", made from a hash of the pilot's key (the key itself never goes over the wire). A realm gets one Citadel; a later pilot's "build the Citadel" step points at the one already standing.

Old saves move over on their own: the save format went to version 8 and every old building became the realm's, with its builder kept.

## The Citadel's tax

Every unload pays the realm 10%. It rounds to the nearest whole unit, takes at least 1, and never the whole load, so a tiny unload still leaves you something. The money goes into a realm wallet, shown at the Citadel and in the HUD.

Press `H` at the Citadel and its screen opens on the REALM tab: the realm wallet, the tax, what you have paid, and buttons to donate 100, 1,000 or everything you carry. The realm wallet will pay for the Citadel's own guns, which are being built now ([coming next](https://ringshadow.com/blog/defences)).

## The realm's tax

- ![The Prospector at the Citadel with the label +24 ORE, 3 TO THE REALM](https://ringshadow.com/blog/media/the-realm/01-tax.webp) An unload at the Citadel: +24 ORE, 3 TO THE REALM.
- ![The Citadel screen on its REALM tab: the realm wallet at 3 ore, the 10% tax, donate buttons](https://ringshadow.com/blog/media/the-realm/02-citadel.webp) The Citadel screen (`H`): the realm wallet holds the 3 ore of tax.
- ![The REALM tab after a donation: 1,003 ore in the realm wallet](https://ringshadow.com/blog/media/the-realm/03-donated.webp) After a donation of 1,000: "Every pilot's Citadel thanks you."

## The shop

The Citadel's second tab is a shop, paid from your own wallet. It sells every blueprint and four supplies. A blueprint is used up by one building, and copies stack, so you can buy three Hab Domes' worth before a trip. Prices rise with the blueprint's level floor and with every copy you buy: a blueprint costs (50 + 25F) × 1.5ⁿ ore and 5F × 1.5ⁿ MH, where F is its level floor and n the copies you already bought. The big ones open late: the Spacedock at level 35, the Bastion at 45, the Sentinel at 55.

| Supply | Key | What it does | Price |
|---|---|---|---|
| Repair kit | `1` | Mends half the Prospector's hp at once | 120 ore, 10 MH |
| MH canister | `2` | 25 MH into the wallet, for jets and flights | 150 ore |
| Shield charge | `3` | No damage taken for 8 s | 100 ore, 30 MH |
| Survey flare | `4` | Sites and deposits nearby show on the map for 60 s | 80 ore, 15 MH |

The shield charge exists because of what came next that same day: [the Brood](https://ringshadow.com/blog/the-brood). Sites in the world still give a free blueprint when you open one, so exploring keeps paying.

## The shop

- ![The SHOP tab: eight blueprints with stacks and prices, four supplies](https://ringshadow.com/blog/media/the-realm/04-shop.webp) The SHOP tab: blueprints on the left, supplies on the right.
- ![The build menu listing each building with the number of blueprint copies held](https://ringshadow.com/blog/media/the-realm/05-build-menu.webp) The build menu counts your copies: Hab Dome ×10.
- ![The Prospector in the open with a shield charge running](https://ringshadow.com/blog/media/the-realm/06-shield.webp) A shield charge running: 8 seconds of no damage.

Under the hood this was three tasks by the agents (R1, C1 and C2), three network protocol versions and three save versions, each with a migration so yesterday's worlds still load. After the shop landed the suites stood at 458 core tests, 167 server tests and 67 network tests, all passing.
