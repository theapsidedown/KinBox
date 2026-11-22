#include <stdio.h>
#include <string.h>

#include "battle.h"
#include "abilities.h"
#include "items.h"
#include "types.h"

#define SPECTRAL_DURATION 5
#define SPECTRAL_ACTIVATION_PCT 25

/* ------------------------------------------------------------------ */
/* Deterministic RNG (identical on both devices)                       */
/* ------------------------------------------------------------------ */

static uint32_t rng_next(battle_t *b)
{
    uint32_t x = b->rng;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    b->rng = x;
    return x;
}

static int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ------------------------------------------------------------------ */
/* Setup                                                               */
/* ------------------------------------------------------------------ */
static void try_spectral_effect(battle_t *b, battle_log *log, int attacker_side);
static void tick_spectral_effects(battle_t *b, battle_log *log);

void battle_mon_from_kindral(battle_mon *m, const kindral *kin)
{
    const kin_species *sp = kindrals_species(kin->id.species);

    memset(m, 0, sizeof(*m));
    snprintf(m->name, sizeof(m->name), "%s", kin->id.name);
    m->species = (uint8_t)kin->id.species;
    m->ability = (uint8_t)kin->id.ability;
    m->level   = (uint8_t)kin->stats.level;
    m->type    = sp ? (uint8_t)sp->type : KIN_TYPE_NONE;
    m->trait   = sp ? (uint8_t)sp->trait : KIN_TRAIT_BEAST;
    m->max_hp  = (int16_t)kin->stats.max_hp;
    m->hp      = (int16_t)clampi(kin->stats.health_points, 1, kin->stats.max_hp);
    m->max_ep  = (int16_t)kin->stats.max_ep;
    m->ep      = (int16_t)clampi(kin->stats.energy_points, 0, kin->stats.max_ep);
    m->atk     = (int16_t)kin->stats.attack;
    m->def     = (int16_t)kin->stats.defense;
    m->spd     = (int16_t)kin->stats.speed;
    memcpy(m->moves, kin->moves.slot, KIN_MAX_MOVES);
}

void battle_init(battle_t *b, const battle_mon *s0, const battle_mon *s1, uint32_t seed)
{
    memset(b, 0, sizeof(*b));
    b->mon[0] = *s0;
    b->mon[1] = *s1;
    b->rng = seed ? seed : 0x9E3779B9u;
    b->result = BATTLE_ONGOING;
}

bool battle_move_usable(const battle_mon *m, int slot)
{
    if (slot < 0 || slot >= KIN_MAX_MOVES) {
        return false;
    }
    const move_def *mv = moves_get(m->moves[slot]);

    return mv != NULL && m->ep >= mv->ep_cost;
}

bool battle_can_afford_any(const battle_mon *m)
{
    for (int i = 0; i < KIN_MAX_MOVES; i++) {
        if (battle_move_usable(m, i)) {
            return true;
        }
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static void emit(battle_t *b, battle_log *log, enum battle_event_kind kind,
                 int side, int id, int val)
{
    if (log->n >= BATTLE_MAX_EVENTS) {
        return;
    }
    battle_event *e = &log->ev[log->n++];

    e->kind = (uint8_t)kind;
    e->side = (uint8_t)side;
    e->id = (uint8_t)id;
    e->val = (int16_t)val;
    for (int i = 0; i < 2; i++) {
        e->hp[i] = b->mon[i].hp;
        e->ep[i] = b->mon[i].ep;
    }
}

static int stage_pct(int stage)
{
    return clampi(100 + 20 * stage, 40, 160);
}

static int eff_stat(const battle_mon *m, enum battle_stat s)
{
    int base = (s == BSTAT_ATK) ? m->atk : (s == BSTAT_DEF) ? m->def : m->spd;

    int value = base * stage_pct(m->stage[s]) / 100;
    if (m->spectral_turns[s] > 0 && m->spectral_reduction[s] > 0) {
        value = value * (100 - m->spectral_reduction[s]) / 100;
    }
    return value;
}

static int eff_speed(const battle_mon *m)
{
    int spd = eff_stat(m, BSTAT_SPD);
    const ability_def *a = abilities_get((enum kindral_ability)m->ability);

    if (a->speed_pct) {
        spd = spd * a->speed_pct(m) / 100;
    }
    return spd;
}

static void faint_check(battle_t *b, battle_log *log, int side)
{
    if (b->mon[side].hp <= 0) {
        b->mon[side].hp = 0;
        emit(b, log, BEV_FAINT, side, 0, 0);
    }
}

static bool someone_down(const battle_t *b)
{
    return b->mon[0].hp <= 0 || b->mon[1].hp <= 0;
}

/* Apply damage to 'side', honouring survive-once abilities. */
static int hurt(battle_t *b, battle_log *log, int side, int dmg)
{
    battle_mon *m = &b->mon[side];
    const ability_def *a = abilities_get((enum kindral_ability)m->ability);

    if (dmg >= m->hp && a->survive_ko_once && !m->survived && m->hp > 1) {
        dmg = m->hp - 1;
        m->survived = true;
        m->hp = 1;
        emit(b, log, BEV_SURVIVE, side, m->ability, 0);
        return dmg;
    }
    if (dmg > m->hp) {
        dmg = m->hp;
    }
    m->hp = (int16_t)(m->hp - dmg);
    return dmg;
}

/* ------------------------------------------------------------------ */
/* Spectral reduces one opposing battle stat without changing permanent stats. */
static int spectral_reduction_pct(int level_difference)
{
    if (level_difference <= 0) return 5;
    if (level_difference <= 4) return 10;
    if (level_difference <= 9) return 15;
    if (level_difference <= 14) return 20;
    return 25;
}

static void try_spectral_effect(battle_t *b, battle_log *log, int attacker_side)
{
    battle_mon *att = &b->mon[attacker_side];
    battle_mon *target = &b->mon[attacker_side ^ 1];
    int eligible[BSTAT_COUNT];
    int count = 0;

    if (att->trait != KIN_TRAIT_SPECTRAL || target->hp <= 0 || att->level < target->level) return;
    for (int stat = BSTAT_ATK; stat <= BSTAT_SPD; stat++) {
        if (target->spectral_turns[stat] == 0) eligible[count++] = stat;
    }
    if (count == 0 || rng_next(b) % 100u >= SPECTRAL_ACTIVATION_PCT) return;

    int stat = eligible[rng_next(b) % (uint32_t)count];
    int reduction = spectral_reduction_pct((int)att->level - (int)target->level);
    target->spectral_reduction[stat] = (int8_t)reduction;
    target->spectral_turns[stat] = SPECTRAL_DURATION;
    emit(b, log, BEV_SPECTRAL, attacker_side ^ 1, stat, reduction);
}

static void tick_spectral_effects(battle_t *b, battle_log *log)
{
    for (int side = 0; side < 2; side++) {
        battle_mon *m = &b->mon[side];
        for (int stat = 0; stat < BSTAT_COUNT; stat++) {
            if (m->spectral_turns[stat] == 0) continue;
            if (--m->spectral_turns[stat] == 0) {
                m->spectral_reduction[stat] = 0;
                emit(b, log, BEV_SPECTRAL_END, side, stat, 0);
            }
        }
    }
}

/* Actions                                                             */
/* ------------------------------------------------------------------ */

static int compute_damage(battle_t *b, int s, const move_def *mv, int *eff_out)
{
    const battle_mon *att = &b->mon[s];
    const battle_mon *def = &b->mon[s ^ 1];
    const ability_def *aa = abilities_get((enum kindral_ability)att->ability);
    const ability_def *da = abilities_get((enum kindral_ability)def->ability);

    int a = eff_stat(att, BSTAT_ATK);
    int d = eff_stat(def, BSTAT_DEF);

    if (d < 1) {
        d = 1;
    }
    int dmg = (int)mv->power * a / d;

    dmg = dmg * 2 / 7 + 1;
    dmg = dmg * (att->level + 10) / 10;

    int eff = types_effectiveness_pct((enum kindral_types)mv->type, (enum kindral_types)def->type);

    dmg = dmg * eff / 100;
    if (mv->type == att->type) {            /* same-type bonus */
        dmg = dmg * 120 / 100;
    }
    if (aa->out_dmg_pct) {
        dmg = dmg * aa->out_dmg_pct(att, mv) / 100;
    }
    if (da->in_dmg_pct) {
        dmg = dmg * da->in_dmg_pct(def, mv) / 100;
    }
    dmg = dmg * (int)(85 + rng_next(b) % 16) / 100;
    if (dmg < 1) {
        dmg = 1;
    }
    *eff_out = eff;
    return dmg;
}

static void do_move(battle_t *b, battle_log *log, int s, int slot)
{
    battle_mon *me = &b->mon[s];
    battle_mon *foe = &b->mon[s ^ 1];
    uint8_t id = (slot >= 0 && slot < KIN_MAX_MOVES) ? me->moves[slot] : MOVE_NONE;
    const move_def *mv = moves_get(id);

    if (mv == NULL || me->ep < mv->ep_cost) {
        if (mv != NULL) {
            emit(b, log, BEV_NO_EP, s, id, 0);
        }
        id = MOVE_STRUGGLE;
        mv = moves_get(id);
    }

    me->ep = (int16_t)(me->ep - mv->ep_cost);
    emit(b, log, BEV_USED, s, id, 0);

    const ability_def *ma = abilities_get((enum kindral_ability)me->ability);
    bool self_stat = (mv->cat == MOVE_CAT_STAT && mv->on_self) || mv->cat == MOVE_CAT_HEAL;
    int acc = clampi(mv->accuracy + ma->accuracy_bonus, 1, 100);

    if (!self_stat && (int)(rng_next(b) % 100) >= acc) {
        emit(b, log, BEV_MISS, s, id, 0);
        return;
    }

    switch (mv->cat) {
    case MOVE_CAT_DAMAGE: {
        int eff;
        int dmg = hurt(b, log, s ^ 1, compute_damage(b, s, mv, &eff));

        if (eff > 100) {
            emit(b, log, BEV_SUPER, s ^ 1, id, 0);
        } else if (eff < 100) {
            emit(b, log, BEV_RESIST, s ^ 1, id, 0);
        }
        emit(b, log, BEV_DAMAGE, s ^ 1, id, dmg);

        const ability_def *da = abilities_get((enum kindral_ability)foe->ability);

        if (mv->drain_pct && dmg > 0) {
            int heal = clampi(dmg * mv->drain_pct / 100, 1, me->max_hp - me->hp);

            if (heal > 0) {
                me->hp = (int16_t)(me->hp + heal);
                emit(b, log, BEV_HEAL, s, id, heal);
            }
        }
        if (da->reflect_pct && dmg > 0 && me->hp > 0) {
            int r = hurt(b, log, s, clampi(dmg * da->reflect_pct / 100, 1, 9999));

            emit(b, log, BEV_RECOIL, s, foe->ability, r);
        }
        if (da->drain_ep_on_hit && dmg > 0) {
            int lost = clampi(da->drain_ep_on_hit, 0, me->ep);

            me->ep = (int16_t)(me->ep - lost);
            emit(b, log, BEV_EP_DRAIN, s, foe->ability, lost);
        }
        if (mv->recoil) {
            int r = hurt(b, log, s, clampi(me->max_hp / 8, 1, 9999));

            emit(b, log, BEV_RECOIL, s, id, r);
        }
        faint_check(b, log, s ^ 1);
        faint_check(b, log, s);
        if (dmg > 0 && foe->hp > 0) try_spectral_effect(b, log, s);
        break;
    }
    case MOVE_CAT_EP_DRAIN: {
        int lost = clampi(mv->power, 0, foe->ep);

        foe->ep = (int16_t)(foe->ep - lost);
        emit(b, log, BEV_EP_DRAIN, s ^ 1, id, lost);
        if (lost > 0) try_spectral_effect(b, log, s);
        break;
    }
    case MOVE_CAT_HEAL: {
        int heal = clampi(me->max_hp * mv->power / 100, 0, me->max_hp - me->hp);

        me->hp = (int16_t)(me->hp + heal);
        emit(b, log, BEV_HEAL, s, id, heal);
        break;
    }
    case MOVE_CAT_STAT: {
        battle_mon *t = mv->on_self ? me : foe;
        int ts = mv->on_self ? s : (s ^ 1);
        int before = t->stage[mv->stat];

        t->stage[mv->stat] = (int8_t)clampi(before + mv->stages, -3, 3);
        emit(b, log, BEV_STAT, ts, mv->stat, t->stage[mv->stat] - before);
        if (!mv->on_self && t->stage[mv->stat] != before) try_spectral_effect(b, log, s);
        break;
    }
    }
}

static void do_item(battle_t *b, battle_log *log, int s, uint8_t item)
{
    battle_mon *me = &b->mon[s];
    const item_def *it = items_get(item);

    emit(b, log, BEV_ITEM, s, item, 0);
    if (it == NULL || !(it->flags & ITEM_F_BATTLE)) {
        return;
    }

    switch (it->kind) {
    case ITEM_K_HEAL_HP: {
        int h = clampi(it->value, 0, me->max_hp - me->hp);

        me->hp = (int16_t)(me->hp + h);
        emit(b, log, BEV_HEAL, s, item, h);
        break;
    }
    case ITEM_K_HEAL_EP: {
        int e = clampi(it->value, 0, me->max_ep - me->ep);

        me->ep = (int16_t)(me->ep + e);
        emit(b, log, BEV_EP_GAIN, s, item, e);
        break;
    }
    case ITEM_K_ELIXIR: {
        int h = me->max_hp - me->hp;
        int e = me->max_ep - me->ep;

        me->hp = me->max_hp;
        me->ep = me->max_ep;
        emit(b, log, BEV_HEAL, s, item, h);
        emit(b, log, BEV_EP_GAIN, s, item, e);
        break;
    }
    case ITEM_K_BOOST: {
        int before = me->stage[it->stat];

        me->stage[it->stat] = (int8_t)clampi(before + it->value, -3, 3);
        emit(b, log, BEV_STAT, s, it->stat, me->stage[it->stat] - before);
        break;
    }
    default:
        break;
    }
}

static int action_priority(const battle_t *b, int s, battle_action a)
{
    if (a.kind == BACT_ITEM) {
        return 3;
    }
    if (a.kind == BACT_MOVE && a.arg < KIN_MAX_MOVES) {
        const move_def *mv = moves_get(b->mon[s].moves[a.arg]);

        if (mv != NULL && b->mon[s].ep >= mv->ep_cost) {
            return mv->priority;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Turn resolution                                                     */
/* ------------------------------------------------------------------ */

void battle_resolve_turn(battle_t *b, const battle_action act[2], battle_log *log)
{
    log->n = 0;
    if (b->result != BATTLE_ONGOING) {
        return;
    }

    /* Forfeits end the battle immediately. */
    bool f0 = act[0].kind == BACT_FORFEIT;
    bool f1 = act[1].kind == BACT_FORFEIT;

    if (f0 || f1) {
        if (f0) {
            emit(b, log, BEV_FORFEIT, 0, 0, 0);
        }
        if (f1) {
            emit(b, log, BEV_FORFEIT, 1, 0, 0);
        }
        b->result = (f0 && f1) ? BATTLE_DRAW : (f0 ? BATTLE_WIN_1 : BATTLE_WIN_0);
        return;
    }

    tick_spectral_effects(b, log);

    /* Turn order: priority, then speed, then a coin flip. */
    int p0 = action_priority(b, 0, act[0]);
    int p1 = action_priority(b, 1, act[1]);
    int first;

    if (p0 != p1) {
        first = (p0 > p1) ? 0 : 1;
    } else {
        int s0 = eff_speed(&b->mon[0]);
        int s1 = eff_speed(&b->mon[1]);

        if (s0 != s1) {
            first = (s0 > s1) ? 0 : 1;
        } else {
            first = (int)(rng_next(b) & 1u);
        }
    }

    for (int i = 0; i < 2 && !someone_down(b); i++) {
        int s = (i == 0) ? first : (first ^ 1);

        if (act[s].kind == BACT_ITEM) {
            do_item(b, log, s, act[s].arg);
        } else {
            do_move(b, log, s, act[s].arg);
        }
    }

    /* End of turn: passive regen and ability effects. */
    if (!someone_down(b)) {
        for (int s = 0; s < 2; s++) {
            battle_mon *m = &b->mon[s];
            const ability_def *a = abilities_get((enum kindral_ability)m->ability);
            int ep = clampi(BATTLE_EP_REGEN + a->end_turn_ep, 0, m->max_ep - m->ep);

            if (ep > 0) {
                m->ep = (int16_t)(m->ep + ep);
                emit(b, log, BEV_EP_GAIN, s, 0, ep);
            }
            if (a->end_turn_heal_pct) {
                int h = clampi(m->max_hp * a->end_turn_heal_pct / 100, 1, m->max_hp - m->hp);

                if (h > 0 && m->hp < m->max_hp) {
                    m->hp = (int16_t)(m->hp + h);
                    emit(b, log, BEV_ABILITY, s, m->ability, h);
                }
            }
            m->turns++;
        }
    }

    b->turn++;

    bool d0 = b->mon[0].hp <= 0;
    bool d1 = b->mon[1].hp <= 0;

    if (d0 || d1) {
        b->result = (d0 && d1) ? BATTLE_DRAW : (d0 ? BATTLE_WIN_1 : BATTLE_WIN_0);
    } else if (b->turn >= BATTLE_MAX_TURNS) {
        emit(b, log, BEV_TIMEOUT, 0, 0, 0);
        b->result = BATTLE_DRAW;
    }
}

uint16_t battle_hash(const battle_t *b)
{
    uint32_t h = 2166136261u;
    const uint32_t vals[] = {
        (uint32_t)b->mon[0].hp, (uint32_t)b->mon[0].ep, (uint32_t)b->mon[1].hp,
        (uint32_t)b->mon[1].ep, b->rng, b->turn, b->result,
        (uint32_t)(uint8_t)b->mon[0].stage[0] | ((uint32_t)(uint8_t)b->mon[0].stage[1] << 8) |
        ((uint32_t)(uint8_t)b->mon[0].stage[2] << 16),
        (uint32_t)(uint8_t)b->mon[1].stage[0] | ((uint32_t)(uint8_t)b->mon[1].stage[1] << 8) |
        ((uint32_t)(uint8_t)b->mon[1].stage[2] << 16),
        (uint32_t)(uint8_t)b->mon[0].spectral_reduction[0] |
        ((uint32_t)(uint8_t)b->mon[0].spectral_reduction[1] << 8) |
        ((uint32_t)(uint8_t)b->mon[0].spectral_reduction[2] << 16),
        (uint32_t)b->mon[0].spectral_turns[0] |
        ((uint32_t)b->mon[0].spectral_turns[1] << 8) |
        ((uint32_t)b->mon[0].spectral_turns[2] << 16),
        (uint32_t)(uint8_t)b->mon[1].spectral_reduction[0] |
        ((uint32_t)(uint8_t)b->mon[1].spectral_reduction[1] << 8) |
        ((uint32_t)(uint8_t)b->mon[1].spectral_reduction[2] << 16),
        (uint32_t)b->mon[1].spectral_turns[0] |
        ((uint32_t)b->mon[1].spectral_turns[1] << 8) |
        ((uint32_t)b->mon[1].spectral_turns[2] << 16),
    };

    for (size_t i = 0; i < sizeof(vals) / sizeof(vals[0]); i++) {
        h = (h ^ vals[i]) * 16777619u;
    }
    return (uint16_t)(h ^ (h >> 16));
}

/* ------------------------------------------------------------------ */
/* Text + rewards                                                      */
/* ------------------------------------------------------------------ */

static const char *const stat_names[BSTAT_COUNT] = { "Attack", "Defense", "Speed" };

int battle_event_text(const battle_t *b, const battle_event *e, char *buf, size_t n)
{
    const char *who = b->mon[e->side & 1].name;
    const move_def *mv = moves_get(e->id);
    const item_def *it = items_get(e->id);

    switch (e->kind) {
    case BEV_USED:    return snprintf(buf, n, "%s used %s!", who, mv ? mv->name : "?");
    case BEV_MISS:    return snprintf(buf, n, "%s missed!", who);
    case BEV_NO_EP:   return snprintf(buf, n, "%s is out of EP!", who);
    case BEV_DAMAGE:  return snprintf(buf, n, "%s took %d damage.", who, e->val);
    case BEV_SUPER:   return snprintf(buf, n, "It's super effective!");
    case BEV_RESIST:  return snprintf(buf, n, "It's not very effective...");
    case BEV_HEAL:    return snprintf(buf, n, "%s regained %d HP.", who, e->val);
    case BEV_EP_DRAIN:return snprintf(buf, n, "%s lost %d EP!", who, e->val);
    case BEV_EP_GAIN: return snprintf(buf, n, "%s regained %d EP.", who, e->val);
    case BEV_STAT:
        return snprintf(buf, n, "%s's %s %s!", who,
                        e->id < BSTAT_COUNT ? stat_names[e->id] : "?",
                        e->val > 0 ? "rose" : (e->val < 0 ? "fell" : "won't change"));
    case BEV_ITEM:    return snprintf(buf, n, "%s used %s.", who, it ? it->name : "an item");
    case BEV_ABILITY: return snprintf(buf, n, "%s's %s healed it.", who,
                                      abilities_get((enum kindral_ability)e->id)->name);
    case BEV_RECOIL:  return snprintf(buf, n, "%s was hurt by recoil! (-%d)", who, e->val);
    case BEV_SURVIVE: return snprintf(buf, n, "%s hung on with %s!", who,
                                      abilities_get((enum kindral_ability)e->id)->name);
    case BEV_FAINT:   return snprintf(buf, n, "%s fainted!", who);
    case BEV_FORFEIT: return snprintf(buf, n, "%s gave up!", who);
    case BEV_TIMEOUT: return snprintf(buf, n, "The battle drags on...");
    case BEV_SPECTRAL:
        return snprintf(buf, n, "%s's %s fell by %d%%!", who,
                        e->id < BSTAT_COUNT ? stat_names[e->id] : "stat", e->val);
    case BEV_SPECTRAL_END:
        return snprintf(buf, n, "%s's %s returned to normal.", who,
                        e->id < BSTAT_COUNT ? stat_names[e->id] : "stat");
    default:          return snprintf(buf, n, "...");
    }
}

int battle_exp_reward(const battle_mon *winner, const battle_mon *loser, bool won)
{
    (void)winner;
    return won ? 20 + 6 * loser->level : 6 + 2 * loser->level;
}
