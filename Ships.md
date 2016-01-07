﻿#summary All one needs to know about ships

# Design #
The first stage is to design your ship. See [UserInterface](UserInterface.md) for a UI concept.

Ships are composed of several component dimensions:
  * **Type** determines the appearance of the ship
  * **Cost** is the amount of gold paid to the Shipyard to construct the base hull
  * **Sails** is the area covered by the ship's sails (minimum amount)
  * **Hull** is the number of Roundshot hits the ship can take before sinking
  * **Crew** is the number of crew needed on board for full efficiency
  * **Cannons** is the maximum number of cannon that can be equipped at a time
Adding armor and sails will further raise the cost of the ship. Adding more cargo and armor plate to a ship will lower its sailing speed.

## Hulls ##
Balance is important, no one hull should be all-around "better".

| **Type**    | **Cost** | **Cargo** | **Sails** | **Hull** | **Crew** | **Cannons** |
|:------------|:---------|:----------|:----------|:---------|:---------|:------------|
| _Sloop_     | 1000     | 60        | **100**   | 40       | 20       | 12          |
|             | A fast, maneuverable vessel useful for scouting.              | | | | | |
| _Brig_      | 2000     | 120       | 80        | 140      | 60       | 18          |
|             | A medium-size, jack-of-all-trades vessel.                     | | | | | |
| _Fluyt_     | 2000     | **240**   | 60        | 100      | 40       | 15          |
|             | A spacious but vulnerable merchant ship.                      | | | | | |
| _Galleon_   | 3000     | 200       | 60        | **200**  | 60       | **40**      |
|             | A sturdy, multi-purpose, but slow-moving ship.                | | | | | |
| _Frigate_   | 5000     | 160       | 90        | 160      | **100**  | **40**      |
|             | A large vessel built exclusively for warfare.                 | | | | | |

**Speed** is determined by the proportion of the sail area to the weight of the hull, armor, and cargo aboard scaled by efficiency. Additional sails can double the speed of a ship. In order of effective speed (`Sails / (Cargo + Hull)`):
  1. Sloop (1.00)
  1. Brig (0.31)
  1. Frigate (0.28)
  1. Fluyt (0.18)
  1. Galleon (0.15)

**Combat potential** depends on how fast a ship deals damage and how much damage it can absorb. Reload time is proportional to the crew size (100 crew fires faster than 60 crew). In order of effective combat potential (`Hull * Crew * Cannons`), normalized by Frigate:
  1. Frigate (1.00)
  1. Galleon (0.75)
  1. Brig (0.24)
  1. Fluyt (0.09)
  1. Sloop (0.02)

**Cargo** room is needed for cannonballs, crew, and cannon as well as goods. Each takes one cargo unit but ammo provides 32 shots per unit. In order of free cargo space (`Cargo - Crew - Cannons`):
  1. Fluyt (185)
  1. Galleon (100)
  1. Brig (42)
  1. Sloop (28)
  1. Frigate (20)

## Variable Components ##
These components can be added in varying amounts by adjusting a slider and have a per-unit cost:
  * **Armor Plates** (Armor Plates) can be used to further reinforce the hull up to double the hull strength
  * **Sails** (Sails) can be adjusted up to double the minimum amount
  * **Cannons** (Cannons) can be loaded up to the maximum amount

## Accessories ##
Each ship may optionally be fitted with up to one of each of the following:
  * **Fishing Nets** (? Rope) allow the ship to harvest fish
  * **Crow's Nest** (? Wood) extends the sight radius of the ship by one tile
  * **Craftsman** (500 Gold) can slowly repair the ship at sea using on-board materials
  * **Towing Hook** (? Steel) allows the ship to tow other ships

# Building and Outfitting #
After you have a ship designed, select the Shipyard and instruct it to build. The Shipyard will only build a basic hull for your ship at first and place it in a nearby tile. The partial ship will then be gradually constructed from the materials you have available in the same way a critically damaged ship is repaired.

After the hull is complete, the Shipyard will attempt to outfit your ship with the parts that the design calls for. As before, the parts will be taken from your Warehouses that are on the island. If a Shipyard does not have the parts it needs to build a ship, it informs the player but does nothing else until the parts are in the warehouse.

At any point during the construction, you can cancel the "repairs" and use another of your ships to tow your ship elsewhere, such as another island's shipyard. Of course, transporting half-built ships is risky business if there is fear of attack.

# Selling Ships #
After a ship is built, it can be sold by marking it as for sale and setting a fixed price. Another player can then purchase the ship if he or she brings a unit nearby or has land on the nearby island.

The benefit of being able to sell ships is that a particularly wealthy and influential player would be able to go entirely into the ship building industry, and sell his products to the other players. It also lets the other players not have to worry so much about creating all the infrastructure needed to build a ship if they don't want to.

# Combat #
To engage in combat, two ships must sail within cannon range (varies for ammunition type). Shots can then be exchanged. The amount of reload time is proportional to the crew size and number of cannons.

Damage is dealt per-cannonball in the following manner:
  * Decide what area of the ship was hit:
    * Probability depends on ammunition type:
      * **Roundshot**: 10% sails / 20% crew / 70% hull
      * **Chainshot**: 70% sails / 10% crew / 20% hull
      * **Grapeshot**: 20% sails / 70% crew / 10% hull
  * If sails were hit:
    * Deal one point of damage to sails and stop here.
  * If the crew was hit:
    * Destroy one unit of Crew and stop here.
  * If the hull was hit:
    * Decide if an armor plate was hit:
      * This chance is the number of plates divided by the maximum hull strength.
      * If an armor plate was hit, destroy one armor plate and stop here.
    * Otherwise, deal one point of damage to the hull and destroy one random unit of cargo.