# Combat: attack, defense, and wounds

This page gives the fight rules of the game as the port runs them: what a hit of a champion does, what a hit on a champion does, and which statistics, skills, and items count. The code is in `src/engine/champion_damage.c`, `src/engine/champion_action.c`, `src/engine/champion.c`, `src/engine/projectile.c`, and `src/engine/creature.c`. The flow of an action is in [champions.md](champions.md), the creature data and the projectiles are in [creatures.md](creatures.md).

Notation: `roll(n)` is a random number from 0 to n. All divisions round down. Weights are in tenths of a kilogram, as in the game data (5.2 KG is 52).

## Summary for the player

- **Melee damage** comes from the *hit strength* of the champion: the strength statistic, twice the level of the skill that the action uses, the strength value of the weapon, and the weight of the weapon. The damage factor of the chosen action multiplies it, and the defense of the creature is subtracted.
- **Weight helps until the weapon is too heavy for the champion.** Each 0.1 kg adds one point up to 1/16 of the maximum load, then only half a point, and above about 1.5 times that limit each 0.1 kg takes one and a half points off. A stronger champion can use a heavier weapon well.
- **Low stamina and a wounded hand halve the hit strength.** Stamina counts only below half of the maximum.
- **A hit must land first.** The dexterity of the champion, lowered by the carried load, is compared with the dexterity of the creature. Darkness and deeper maps make a hit harder. A miss gets a second chance from luck.
- **The skill level also gives critical hits:** a chance of level/64 to double the damage and add 10.
- **Armor works for each body part.** A helmet protects only the head, a torso armor only the torso, and so on. Each armor point lowers the damage by about 0.8 percent of the attack. A shield in a hand protects every body part a little, and its own hand most.
- **Armor does not stop everything.** Magic attacks are reduced by anti-magic, fire by anti-fire and then by armor, psychic attacks by wisdom. Poison clouds and similar blasts ignore all defenses.
- **A big hit wounds.** Vitality raises the damage that a champion takes without a wound. A wound lowers the defense of that body part, a wounded hand halves its hit strength, and any wound lowers the maximum load.
- **Parry skill** lowers the power of each melee attack of a creature by twice its level.
- **Asleep, the party is weak:** every skill counts as level 1, dexterity and defense are halved, and no melee attack of a creature misses.

## Values that the formulas use

- Statistics: luck, strength, dexterity, wisdom, vitality, anti-magic, anti-fire. Each has a current and a maximum value from 10 to 220. `champ_get_stat` adds the bonus of the worn items, and for a statistic with an aura a random part of the aura.
- Skill levels (`champ_get_skill_level`): level 1 is untrained. Each doubling of the experience from 512 gives one level (512: level 2, 1024: level 3). A sub-skill uses the mean of its own experience and the experience of its base skill; a skill bonus of an item on the base skill counts the base experience once more for each point. The bonus of an item on the skill itself is added to the level.
- Maximum load (`champ_get_max_load`): strength × 8 + 100, scaled by the stamina (below), less 1/8 with a wound, or less 1/4 with a wounded leg, rounded up to a full kilogram.
- Stamina scale (`champ_scale_by_stamina`): with at least half of the maximum stamina a value stays. Below, the value is value/2 × (1 + stamina / half of the maximum): from the full value at half stamina down to half the value at no stamina.
- Map difficulty: a value of each map (`dungeon_map_difficulty`). The formulas below use `difficulty` for twice that value.
- Light level (`g_view_light_level`): 0 is full light, a higher level is darker.
- Item attributes ([GDAT](../formats/graphics.md#gdat-items) values of an item, `src/engine/include/item.h`): weight, strength (`ATTR_STRENGTH`), throw power (`ATTR_THROW_POWER`), shoot attack (`ATTR_SHOOT_ATTACK`), armor class (`ATTR_ARMOUR_CLASS`: the defense in bits 0 to 7, the sharp factor 0 to 7 in bits 8 to 10, and bit 15 for a hand item that defends), and extra damage (`ATTR_EXTRA_DAMAGE`).
- Action fields (the text of an action of an item): skill, delay, stamina, defense, attack (the hit chance value), and the damage factor (`FIELD_WOUNDS`, in 32nds).
- Creature values (`CreatureInfo`): defense, dexterity, attack, attack kind, poison, armor (0 to 15), and the wound chances.

## Hit strength of a champion

`champ_get_strength(champion, hand, skill)` gives a value from 0 to 100 for the item in a hand:

1. `v = roll(15) + strength + weight - 12`.
2. `light = maximum load / 16`. If the weight is above `light`: `v -= (weight - light) / 2`. `heavy = light + (light - 12) / 2`. If the weight is above `heavy`: `v -= 2 × (weight - heavy)`.
3. `v += 2 × skill level`.
4. The item bonus:
   - For the fighter skill, skills 4 to 7 (swing to parry), and skill 9: the strength attribute of the item.
   - For the ninja skill and for throwing: the throw power of the item, but not of a launcher.
   - For shooting: the throw power of the item, only of a launcher.
5. The stamina scale applies to `v`.
6. A wound on that hand halves `v`.
7. The result is `v / 2`, limited to 0 to 100.

The same value breaks doors (a melee action on a closed door) and starts a throw.

## Dexterity and luck

`champ_get_dexterity(champion)`:

1. `v = roll(7) + dexterity`.
2. `v -= (v / 2) × load / maximum load`: a full load takes half.
3. `v` is at least 2. Asleep, `v` is halved.
4. The result is `v / 2`, not below `roll(7) + 1` and not above `100 - roll(7)`.

`champ_test_luck(champion, chance)` gives a second chance; a lower `chance` is easier:

1. In half of the cases a roll of 0 to 99 above `chance` succeeds, and the luck does not change.
2. Else `roll(2 × luck - 1)` above `chance` succeeds. A success lowers the current luck by 2, a failure raises it by 2 (limits: 10, and the maximum luck up to 220).

## A melee hit of a champion

`melee_attack_group` in `src/engine/champion_action.c`.

No hit is possible when the creature cannot be hit (dexterity 255), when the creature acts in the same game step, or when the creature is non-material and the action does not pierce. A champion in a back cell cannot attack while a champion stands in front of them.

The hit roll:

1. `chance = (roll(31) + creature dexterity + difficulty + 2 × light level - 16) / 2`.
2. A champion dexterity (above) greater than `chance` hits.
3. Else 1 case of 4 hits anyway, and the other 3 take a luck test with `75 - attack`, where `attack` is the attack field of the action.

The base damage (`melee_hit_damage`), with the hit strength `S` of the hand and the skill of the action:

1. `d = S + roll(S / 2)`.
2. `d = d × damage factor / 32`.
3. `d = d + roll(31) - (roll(31) + creature defense + difficulty)`.
4. A `d` above 1 stays. Else the hit is weak: 1 case of 4 misses, the others do 2 to 4 damage, sometimes a little more.

The final damage:

1. `d = d / 2`, then `d += roll(d - 1) + roll(3)`, then `d += roll(d - 1)`, then `d = d / 4 + roll(3) + 1`. The mean is about 0.28 of the base damage plus 3.
2. Critical hit: if the skill level is greater than `roll(63)`, `d = 2 × d + 10`.
3. Extra damage of the weapon: if `d` is greater than `roll(31)`, `d += (extra damage + roll(3)) × 8 / (creature armor + 2)`. A creature with armor 15 takes none.
4. A creature that resists non-fire attacks takes a half or a quarter, except from an action with melee kind 1.

Cost and gain:

- The action takes its stamina field plus `roll(1)`. A hit takes 4 to 7 more stamina, a miss 2 to 3.
- The experience of a hit is the experience factor of the creature × damage / 16 + 3, in the skill of the action, besides the experience field of the action.
- The delay of the hand is the delay field × 1.25 + 2 game steps (with haste: a quarter of the first part), added to half of a delay that still runs.

## Throwing and shooting

A throw (`throw_object` in `src/engine/champion_action.c`), with the throwing level `L`:

- Energy: the hit strength with the throwing skill, plus the throw power of the item (1 without one), plus `roll(energy / 4 + 7)`, plus `L`; at most 255.
- Hit chance value: `roll(31) + 8 × L`, limited to 40 to 200.
- Flight step: the step attribute of the item, else `11 - L`, at least 5.
- Stamina: half the weight, at most 10, plus half of each further part of 10.

A shot (`run_shoot`), with the shooting level `L`:

- Energy: `L` + the throw power of the launcher + the throw power of the ammunition.
- Hit chance value: the shoot attack of the launcher + `2 × L`.

The attack of a flying object when it hits (`roll_attack` in `src/engine/projectile.c`), with the energy that is left:

1. With a throw power: `factor = hit chance / 16 + 3` and `power = (throw power + energy / 2) × factor × factor / 128`. The attack is sharp. Without a throw power the power is 0 and the attack is blunt.
2. `power += roll(3) + weight`. If the hit chance is greater than `roll(511)`, the power doubles.
3. `attack = (power + energy) / 16 + 1`, then `attack += roll(attack / 2) + roll(3)`.
4. With `bonus = 32 - hit chance / 8`, the attack becomes the greater of `attack` and `(attack - bonus) × 2`, but at most `2 × energy`.

On a creature (`hit_creature`): `damage = attack × 64 / creature defense`. The extra damage of the object is added as in a melee hit, `(extra damage + roll(3)) × 8 / (creature armor + 2)`. A creature that resists non-fire attacks takes a quarter. There is no miss roll: an object that meets the creature hits it, but a non-material creature is hit only by the spell against non-material creatures.

On a champion (`hit_champion`): `champ_take_damage` takes the attack for the head and the torso, with the attack type of the missile (sharp or blunt for an object, fire, lightning, or magic for a spell). A champion who faces the missile can block it with a parry. A missile with extra damage poisons the champion in 7 cases of 8 when the hit did damage.

## A melee attack of a creature on a champion

`projectile_attack_champion` in `src/engine/projectile.c`.

The dodge:

1. The creature dexterity is doubled for attack kind 9 (at most 255). Attack kind 8 always hits. A sleeping party never dodges.
2. `threshold = visibility + roll(31) + creature dexterity + difficulty`. The visibility is 16 for an invisible party that the creature cannot see, 0 for a creature that sees in the dark, else twice the light level.
3. The champion dodges when the champion dexterity is at least `threshold - 16`, in 3 cases of 4. Else a luck test with 60 can still dodge.

The body part: 1 hit of 8 goes to a hand, as do all hits of a creature without wound chances. The other hits go to the feet, the legs, the torso, or the head by the wound chances of the creature.

The power:

1. `power = attack + min(attack, roll(15) + difficulty)`.
2. Except for attack kind 8: `power -= 2 × parry level`. If the power is then 1 or less, half of the attacks do nothing and the others get a power of 2 to 5.
3. `power = power / 2`, then `power += roll(power - 1) + roll(3)`, then `power += roll(power - 1)`, then `power = power / 4 + roll(3) + 1`. In half of the cases a random part up to half is taken off.
4. `champ_take_damage` takes the power, the body part, and the attack kind of the creature as the attack type.

Poison: after a hit with damage, a creature with poison poisons the champion in half of the cases, with `poison × (170 - vitality) / 128`.

## Damage to a champion

`champ_take_damage(champion, damage, wound locations, attack type)` in `src/engine/champion_damage.c`. The damage and the wounds are applied at the end of the game step.

Attack types: 1 fire, 2 and 8 self (falls, bumps, closing doors), 3 blunt, 4 sharp, 5 magic, 6 psychic, 7 lightning. Type 0 (exhaustion, poison, poison clouds and other blasts on the party square) takes the full damage with no defense and no wound.

For the other types `reduce_damage` runs these steps:

1. The defense is the mean of the defense of the body parts that the attack can hit (below).
2. Parry: the parry value is the sum of the defense fields of the hands whose last action was a parry. If `parry level + parry value / 8` is greater than `roll(15)`, the parry works: a blockable attack (a missile that the champion faces) loses the parry value, and the defense gains a quarter of it.
3. By type:
   - Fire: `d = d × (170 - anti-fire) / 128`, or `d / 8` for an anti-fire above 154; then minus the value of a fire shield. Then the armor step.
   - Self: the defense becomes `defense / 2 + ninja level`. Then the armor step.
   - Magic: `d = d × (170 - anti-magic) / 128`, or `d / 8` for an anti-magic above 154; then minus the value of a spell shield. No armor step.
   - Psychic: `d = d × (115 - wisdom) / 64`; a wisdom of 115 or more takes no damage. No armor step.
   - Blunt, sharp, lightning: only the armor step.
4. The armor step: `d = d × (130 - defense) / 64`.

The armor step doubles the damage at defense 0, leaves it at defense 66, and halves it at defense 98. Each point of defense is worth 1/64 of the attack.

The defense of a body part (`wound_defense`), 0 to 100:

1. Each hand item with the defends bit (a shield) adds `(armor defense + hit strength of that hand with the parry skill) × part factor`, divided by 16 for its own hand and by 32 for every other body part. The part factors are: ready hand 5, action hand 5, head 4, torso 6, legs 3, feet 1.
2. Plus `roll(vitality / 8)`, halved against a sharp attack.
3. Plus the value of a defense aura (the party shield).
4. Plus the defense fields of the last action of both hands. A hand loses its value when its delay ends.
5. For the head, the torso, the legs, and the feet: plus the armor defense of the item in that slot.
6. A wounded body part loses 8 to 11.
7. Asleep, the sum is halved.
8. The result is the sum / 2, limited to 0 to 100.

Against a sharp attack an armor defense counts `defense × (sharp factor + 4) / 8`: a sharp factor of 4 keeps the value, 0 halves it, and 7 raises it to 11/8.

So 10 armor points on a body part give 5 defense, which is 5/64 or about 8 percent of the attack. Only the four body slots and a defending hand item count; a neck item or a pouch gives no armor. An attack that can hit several body parts (a missile: the head and the torso; fire on the party: all six) uses the mean of them.

Wounds: `resist = (roll(127) + 10) × (170 - vitality) / 128`, or an eighth of the roll for a vitality above 154. While the damage is above the resistance, one of 8 picks is drawn, 6 of them the body parts; a body part that the attack can hit gets a wound, and the resistance doubles. An attack with a type also wakes a sleeping party.

Effects of a wound:

- The body part loses 8 to 11 defense.
- A wounded hand halves its hit strength.
- Any wound lowers the maximum load by 1/8, a wounded leg by 1/4.
- Wounded feet slow the party.

Damage to the whole party (`champ_damage_party`): each champion takes the damage minus an eighth plus a random part up to a quarter, and then the steps above.

## Deviations from the original

- The formulas are those of the original. The fixes of the port in this code (the remainder of `melee_hit_damage` for a hit strength of 0, a skill outside the skill table) are in [champions.md](champions.md#deviations-from-the-original).
- The advanced mode shows the damage of a hit over the creature ([advanced-mode.md](advanced-mode.md#hit-feedback-over-creatures)) and can make every hit kill; it changes no formula of this page.
