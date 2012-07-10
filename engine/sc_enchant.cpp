// ==========================================================================
// Dedmonwakeen's Raid DPS/TPS Simulator.
// Send questions to natehieter@gmail.com
// ==========================================================================

#include "simulationcraft.hpp"

namespace { // ANONYMOUS NAMESPACE

static const stat_e reforge_stats[] =
{
  STAT_SPIRIT,
  STAT_DODGE_RATING,
  STAT_PARRY_RATING,
  STAT_HIT_RATING,
  STAT_CRIT_RATING,
  STAT_HASTE_RATING,
  STAT_EXPERTISE_RATING,
  STAT_MASTERY_RATING,
  STAT_NONE
};

// Weapon Stat Proc Callback ================================================

struct weapon_stat_proc_callback_t : public action_callback_t
{
  weapon_t* weapon;
  buff_t* buff;
  double PPM;
  bool all_damage;

  weapon_stat_proc_callback_t( player_t* p, weapon_t* w, buff_t* b, double ppm=0.0, bool all=false ) :
    action_callback_t( p ), weapon( w ), buff( b ), PPM( ppm ), all_damage( all ) {}

  virtual void trigger( action_t* a, void* /* call_data */ )
  {
    if ( ! all_damage && a -> proc ) return;
    if ( weapon && a -> weapon != weapon ) return;

    if ( PPM > 0 )
    {
      buff -> trigger( 1, 0, weapon -> proc_chance_on_swing( PPM ) ); // scales with haste
    }
    else
    {
      buff -> trigger();
    }
    buff -> up();  // track uptime info
  }
};

// Weapon 2-Stat Proc Callback ================================================

struct weapon_2_stat_proc_callback_t : public action_callback_t
{
  weapon_t* weapon;
  buff_t* buff;
  buff_t* buff2;
  double PPM;
  bool all_damage;
  bool (*check_func)( action_t* a );

  weapon_2_stat_proc_callback_t( player_t* p, weapon_t* w, buff_t* b, buff_t* b2, bool (*c)( action_t* a ), double ppm=0.0, bool all=false  ) :
    action_callback_t( p ), weapon( w ), buff( b ), buff2( b2 ), PPM( ppm ), all_damage( all ), check_func( c ) {}

  virtual void trigger( action_t* a, void* /* call_data */ )
  {
    bool res = false;

    if ( ! all_damage && a -> proc ) return;
    if ( weapon && a -> weapon != weapon ) return;

    if ( PPM > 0 )
    {
      res = buff -> trigger( 1, 0, weapon -> proc_chance_on_swing( PPM ) ); // scales with haste
    }
    else
    {
      res = buff -> trigger();
    }
    buff -> up();  // track uptime info

    if ( res && check_func( a ) )
    {
      if ( PPM > 0 )
      {
        buff2 -> trigger( 1, 0, weapon -> proc_chance_on_swing( PPM ) );
      }
      else
      {
        buff2 -> trigger();
      }
      buff2 -> up();
    }
  }
};


// Weapon Discharge Proc Callback ===========================================

struct weapon_discharge_proc_callback_t : public action_callback_t
{
  std::string name_str;
  weapon_t* weapon;
  int stacks, max_stacks;
  double fixed_chance, PPM;
  cooldown_t* cooldown;
  spell_t* spell;
  proc_t* proc;
  rng_t* rng;

  weapon_discharge_proc_callback_t( const std::string& n, player_t* p, weapon_t* w, int ms, school_e school, double dmg, double fc, double ppm=0, timespan_t cd=timespan_t::zero() ) :
    action_callback_t( p ),
    name_str( n ), weapon( w ), stacks( 0 ), max_stacks( ms ), fixed_chance( fc ), PPM( ppm )
  {
    struct discharge_spell_t : public spell_t
    {
      discharge_spell_t( const char* n, player_t* p, double dmg, school_e s ) :
        spell_t( n, p, spell_data_t::nil() )
      {
        school = ( s == SCHOOL_DRAIN ) ? SCHOOL_SHADOW : s;
        trigger_gcd = timespan_t::zero();
        base_dd_min = dmg;
        base_dd_max = dmg;
        may_crit = ( s != SCHOOL_DRAIN );
        background  = true;
        proc = true;
        base_spell_power_multiplier = 0;
        init();
      }
    };

    cooldown = p -> get_cooldown( name_str );
    cooldown -> duration = cd;

    spell = new discharge_spell_t( name_str.c_str(), p, dmg, school );

    proc = p -> get_proc( name_str.c_str() );
    rng  = p -> get_rng ( name_str.c_str() );
  }

  virtual void reset() { stacks=0; }

  virtual void deactivate() { action_callback_t::deactivate(); stacks=0; }

  virtual void trigger( action_t* a, void* /* call_data */ )
  {
    if ( a -> proc ) return;
    if ( weapon && a -> weapon != weapon ) return;

    if ( cooldown -> remains() > timespan_t::zero() )
      return;

    double chance = fixed_chance;
    if ( weapon && PPM > 0 )
      chance = weapon -> proc_chance_on_swing( PPM ); // scales with haste

    if ( chance > 0 )
      if ( ! rng -> roll( chance ) )
        return;

    cooldown -> start();

    if ( ++stacks >= max_stacks )
    {
      stacks = 0;
      spell -> execute();
      proc -> occur();
    }
  }
};

// register_synapse_springs =================================================

void register_synapse_springs( item_t* item )
{
  player_t* p = item -> player;

  if ( p -> profession[ PROF_ENGINEERING ] < 425 )
  {
    item -> sim -> errorf( "Player %s attempting to use synapse springs without 425 in engineering.\n", p -> name() );
    return;
  }

  static const attribute_e attr[] = { ATTR_STRENGTH, ATTR_AGILITY, ATTR_INTELLECT };

  stat_e max_stat = STAT_INTELLECT;
  double max_value = -1;

  for ( unsigned i = 0; i < sizeof_array( attr ); ++i )
  {
    if ( p -> current.attribute[ attr[ i ] ] > max_value )
    {
      max_value = p -> current.attribute[ attr[ i ] ];
      max_stat = stat_from_attr( attr[ i ] );
    }
  }

  item -> use.name_str = "synapse_springs";
  item -> use.stat = max_stat;
  item -> use.stat_amount = 480.0;
  item -> use.duration = timespan_t::from_seconds( 10.0 );
  item -> use.cooldown = timespan_t::from_seconds( 60.0 );
}

// register_synapse_springs_2 =================================================

void register_synapse_springs_2( item_t* item )
{
  player_t* p = item -> player;

  if ( p -> profession[ PROF_ENGINEERING ] < 550 )
  {
    item -> sim -> errorf( "Player %s attempting to use synapse springs mk 2 without 500 in engineering.\n", p -> name() );
    return;
  }

  static const attribute_e attr[] = { ATTR_STRENGTH, ATTR_AGILITY, ATTR_INTELLECT };

  stat_e max_stat = STAT_INTELLECT;
  double max_value = -1;

  for ( unsigned i = 0; i < sizeof_array( attr ); ++i )
  {
    if ( p -> current.attribute[ attr[ i ] ] > max_value )
    {
      max_value = p -> current.attribute[ attr[ i ] ];
      max_stat = stat_from_attr( attr[ i ] );
    }
  }

  item -> use.name_str = "synapse_springs_2";
  item -> use.stat = max_stat;
  item -> use.stat_amount = 2940.0;
  item -> use.duration = timespan_t::from_seconds( 10.0 );
  item -> use.cooldown = timespan_t::from_seconds( 60.0 );
}

// register_synapse_springs_2 =================================================

void register_phase_fingers( item_t* item )
{
  player_t* p = item -> player;

  if ( p -> profession[ PROF_ENGINEERING ] < 500 )
  {
    item -> sim -> errorf( "Player %s attempting to use phase fingers without 500 in engineering.\n", p -> name() );
    return;
  }
  item -> use.name_str = "phase_fingers";
  item -> use.stat = STAT_DODGE_RATING;
  item -> use.stat_amount = 240.0;
  item -> use.duration = timespan_t::from_seconds( 10.0 );
  item -> use.cooldown = timespan_t::from_seconds( 60.0 );
}

void register_avalanche( player_t* p, const std::string& mh_enchant, const std::string& oh_enchant, weapon_t* mhw, weapon_t* ohw )
{
  if ( mh_enchant == "avalanche" || oh_enchant == "avalanche" )
  {
    if ( mh_enchant == "avalanche" )
    {
      action_callback_t* cb = new weapon_discharge_proc_callback_t( "avalanche_mh", p, mhw, 1, SCHOOL_NATURE, 500, 0, 5.0/*PPM*/, timespan_t::from_seconds( 0.01 )/*CD*/ );
      p -> callbacks.register_attack_callback( RESULT_HIT_MASK, cb );
    }
    if ( oh_enchant == "avalanche" )
    {
      action_callback_t* cb = new weapon_discharge_proc_callback_t( "avalanche_oh", p, ohw, 1, SCHOOL_NATURE, 500, 0, 5.0/*PPM*/, timespan_t::from_seconds( 0.01 )/*CD*/ );
      p -> callbacks.register_attack_callback( RESULT_HIT_MASK, cb );
    }
    // Reference: http://elitistjerks.com/f79/t110302-enhsim_cataclysm/p4/#post1832162
    action_callback_t* cb = new weapon_discharge_proc_callback_t( "avalanche_s", p, 0, 1, SCHOOL_NATURE, 500, 0.25/*FIXED*/, 0, timespan_t::from_seconds( 10.0 )/*CD*/ );
    p -> callbacks.register_spell_callback ( RESULT_HIT_MASK, cb );
  }
}

void register_executioner( player_t* p, const std::string& mh_enchant, const std::string& oh_enchant, weapon_t* mhw, weapon_t* ohw )
{
  if ( mh_enchant == "executioner" || oh_enchant == "executioner" )
  {
    // MH-OH trigger/refresh the same Executioner buff.  It does not stack.

    stat_buff_t* buff = stat_buff_creator_t( p, "executioner" )
                        .spell( p -> find_spell( 42976 ) )
                        .cd( timespan_t::zero() )
                        .chance( 0 )
                        .activated( false );

    if ( mh_enchant == "executioner" )
    {
      p -> callbacks.register_attack_callback( RESULT_HIT_MASK, new weapon_stat_proc_callback_t( p, mhw, buff, 1.0/*PPM*/ ) );
    }
    if ( oh_enchant == "executioner" )
    {
      p -> callbacks.register_attack_callback( RESULT_HIT_MASK, new weapon_stat_proc_callback_t( p, ohw, buff, 1.0/*PPM*/ ) );
    }
  }
}

void register_hurricane( player_t* p, const std::string& mh_enchant, const std::string& oh_enchant, weapon_t* mhw, weapon_t* ohw )
{
  if ( mh_enchant == "hurricane" || oh_enchant == "hurricane" )
  {
    stat_buff_t *mh_buff=0, *oh_buff=0;
    if ( mh_enchant == "hurricane" )
    {
      mh_buff = stat_buff_creator_t( p, "hurricane_mh" )
                .spell( p -> find_spell( 74221 ) )
                .cd( timespan_t::zero() )
                .chance( 0 )
                .activated( false );
      p -> callbacks.register_direct_damage_callback( SCHOOL_ATTACK_MASK, new weapon_stat_proc_callback_t( p, mhw, mh_buff, 1.0/*PPM*/, true/*ALL*/ ) );
      p -> callbacks.register_tick_damage_callback  ( SCHOOL_ATTACK_MASK, new weapon_stat_proc_callback_t( p, mhw, mh_buff, 1.0/*PPM*/, true/*ALL*/ ) );
    }
    if ( oh_enchant == "hurricane" )
    {
      oh_buff = stat_buff_creator_t( p, "hurricane_oh" )
                .spell( p -> find_spell( 74221 ) )
                .cd( timespan_t::zero() )
                .chance( 0 )
                .activated( false );
      p -> callbacks.register_direct_damage_callback( SCHOOL_ATTACK_MASK, new weapon_stat_proc_callback_t( p, ohw, oh_buff, 1.0/*PPM*/, true /*ALL*/ ) );
      p -> callbacks.register_tick_damage_callback  ( SCHOOL_ATTACK_MASK, new weapon_stat_proc_callback_t( p, ohw, oh_buff, 1.0/*PPM*/, true /*ALL*/ ) );
    }
    // Custom proc is required for spell damage procs.
    // If MH buff is up, then refresh it, else
    // IF OH buff is up, then refresh it, else
    // Trigger a new buff not associated with either MH or OH
    // This means that it is possible to have three stacks
    struct hurricane_spell_proc_callback_t : public action_callback_t
    {
      buff_t *mh_buff, *oh_buff, *s_buff;
      hurricane_spell_proc_callback_t( player_t* p, buff_t* mhb, buff_t* ohb, buff_t* sb ) :
        action_callback_t( p ), mh_buff( mhb ), oh_buff( ohb ), s_buff( sb )
      {
      }
      virtual void trigger( action_t* /* a */, void* /* call_data */ )
      {
        if ( s_buff -> cooldown -> remains() > timespan_t::zero() ) return;
        if ( ! s_buff -> rng -> roll( 0.15 ) ) return;
        if ( mh_buff && mh_buff -> check() )
        {
          mh_buff -> trigger();
          s_buff -> cooldown -> start();
        }
        else if ( oh_buff && oh_buff -> check() )
        {
          oh_buff -> trigger();
          s_buff -> cooldown -> start();
        }
        else s_buff -> trigger();
      }
    };
    stat_buff_t* s_buff = stat_buff_creator_t( p, "hurricane_s" )
                          .spell( p -> find_spell( 74221 ) )
                          .cd( timespan_t::from_seconds( 45.0 ) )
                          .activated( false );
    p -> callbacks.register_direct_damage_callback( SCHOOL_SPELL_MASK, new hurricane_spell_proc_callback_t( p, mh_buff, oh_buff, s_buff ) );
    p -> callbacks.register_tick_damage_callback  ( SCHOOL_SPELL_MASK, new hurricane_spell_proc_callback_t( p, mh_buff, oh_buff, s_buff ) );
    p -> callbacks.register_direct_heal_callback( SCHOOL_SPELL_MASK, new hurricane_spell_proc_callback_t( p, mh_buff, oh_buff, s_buff ) );
    p -> callbacks.register_tick_heal_callback  ( SCHOOL_SPELL_MASK, new hurricane_spell_proc_callback_t( p, mh_buff, oh_buff, s_buff ) );
  }
}

void register_landslide( player_t* p, const std::string& enchant, weapon_t* w, const std::string& weapon_appendix )
{
  if ( enchant == "landslide" )
  {
    stat_buff_t* buff = stat_buff_creator_t( p, "landslide_" + weapon_appendix )
                        .spell( p -> find_spell( 74245 ) )
                        .activated( false )
                        .add_stat( STAT_ATTACK_POWER, 1000 );
    p -> callbacks.register_attack_callback( RESULT_HIT_MASK, new weapon_stat_proc_callback_t( p, w, buff, 1.0/*PPM*/ ) );
  }
}

void register_mongoose( player_t* p, const std::string& enchant, weapon_t* w, const std::string& weapon_appendix )
{
  if ( enchant == "mongoose" )
  {
    p -> buffs.mongoose_mh = stat_buff_creator_t( p, "mongoose_" + weapon_appendix )
                             .duration( timespan_t::from_seconds( 15 ) )
                             .activated( false )
                             .add_stat( STAT_AGILITY, 120 );
    p -> callbacks.register_attack_callback( RESULT_HIT_MASK, new weapon_stat_proc_callback_t( p, w, p -> buffs.mongoose_mh, 1.0/*PPM*/ ) );
  }
}

void register_power_torrent( player_t* p, const std::string& enchant, const std::string& weapon_appendix )
{
  if ( enchant == "power_torrent" )
  {
    stat_buff_t* buff = stat_buff_creator_t( p, "power_torrent" + weapon_appendix )
                        .spell( p -> find_spell( 74241 ) )
                        .cd( timespan_t::from_seconds( 45 ) )
                        .chance( 0.20 )
                        .activated( false )
                        .add_stat( STAT_INTELLECT, 500 );
    weapon_stat_proc_callback_t* cb = new weapon_stat_proc_callback_t( p, NULL, buff );
    p -> callbacks.register_tick_damage_callback  ( RESULT_ALL_MASK, cb );
    p -> callbacks.register_direct_damage_callback( RESULT_ALL_MASK, cb );
    p -> callbacks.register_tick_heal_callback    ( RESULT_ALL_MASK, cb );
    p -> callbacks.register_direct_heal_callback  ( RESULT_ALL_MASK, cb );
  }
}

// FIX ME: Guessing at proc chance, ICD; not sure how to implement
// conditional spirit buff
static bool jade_spirit_check_func( action_t* a )
{
  if ( a -> player -> resources.max[ RESOURCE_MANA ] <= 0.0 ) return false;

  if ( a -> player -> resources.current[ RESOURCE_MANA ] / a -> player -> resources.max[ RESOURCE_MANA ] < 0.25 )
    return true;

  return false;
}

void register_jade_spirit( player_t* p, const std::string& enchant, const std::string& weapon_appendix )
{
  if ( enchant == "jade_spirit" )
  {
    stat_buff_t* buff  = stat_buff_creator_t( p, "jade_spirit" + weapon_appendix )
                         .duration( timespan_t::from_seconds( 12 ) )
                         .cd( timespan_t::from_seconds( 45 ) )
                         .chance( 0.10 )
                         .activated( false )
                         .add_stat( STAT_INTELLECT, 1650 );
    stat_buff_t* buff2 = stat_buff_creator_t( p, "jade_spirit_spi" + weapon_appendix )
                         .duration( timespan_t::from_seconds( 12 ) )
                         .cd( timespan_t::from_seconds( 45 ) )
                         .chance( 1.0 )
                         .activated( false )
                         .add_stat( STAT_SPIRIT, 750 );

    weapon_2_stat_proc_callback_t* cb = new weapon_2_stat_proc_callback_t( p, NULL, buff, buff2, jade_spirit_check_func );

    p -> callbacks.register_tick_damage_callback  ( RESULT_ALL_MASK, cb );
    p -> callbacks.register_direct_damage_callback( RESULT_ALL_MASK, cb );
    p -> callbacks.register_tick_heal_callback    ( RESULT_ALL_MASK, cb );
    p -> callbacks.register_direct_heal_callback  ( RESULT_ALL_MASK, cb );
  }
}

void register_windwalk( player_t* p, const std::string& enchant, weapon_t* w, const std::string& weapon_appendix )
{
  if ( enchant == "windwalk" )
  {
    stat_buff_t* buff = stat_buff_creator_t( p, "windwalk_" + weapon_appendix )
                        .duration( timespan_t::from_seconds( 10 ) )
                        .cd( timespan_t::from_seconds( 45 ) )
                        .chance( 0.15 )
                        .activated( false )
                        .add_stat( STAT_DODGE_RATING, 600 );
    p -> callbacks.register_attack_callback( RESULT_HIT_MASK, new weapon_stat_proc_callback_t( p, w, buff ) );
  }
}

void register_berserking( player_t* p, const std::string& enchant, weapon_t* w, const std::string& weapon_appendix )
{
  if ( enchant == "berserking" )
  {
    stat_buff_t* buff = stat_buff_creator_t( p, "berserking_" + weapon_appendix )
                        .max_stack( 1 )
                        .duration( timespan_t::from_seconds( 15 ) )
                        .cd( timespan_t::zero() )
                        .chance( 0 )
                        .activated( false )
                        .add_stat( STAT_ATTACK_POWER, 400.0 );
    p -> callbacks.register_attack_callback( RESULT_HIT_MASK, new weapon_stat_proc_callback_t( p, w, buff, 1.0/*PPM*/ ) );
  }
}

void register_gnomish_xray( player_t* p, const std::string& enchant, weapon_t* w )
{
  if ( enchant == "gnomish_xray" )
  {
    //FIXME: 1.0 ppm and 40 second icd seems to roughly match in-game behavior, but we need to verify the exact mechanics
    stat_buff_t* buff = stat_buff_creator_t( p, "xray_targeting" )
                        .spell( p -> find_spell( 95712 ) )
                        .cd( timespan_t::from_seconds( 40 ) )
                        .activated( false );

    p -> callbacks.register_attack_callback( RESULT_HIT_MASK, new weapon_stat_proc_callback_t( p, w, buff, 1.0/*PPM*/ ) );
  }
}

void register_lord_blastingtons_scope_of_doom( player_t* p, const std::string& enchant, weapon_t* w )
{
  if ( enchant == "lord_blastingtons_scope_of_doom" )
  {
    //FIXME: Using gnomish x-ray proc and icd for now. CONFIRM.
    stat_buff_t* buff = stat_buff_creator_t( p, "lord_blastingtons_scope_of_doom" )
                        .spell( p -> find_spell( 109085 ) )
                        .cd( timespan_t::from_seconds( 40 ) )
                        .activated( false );

    p -> callbacks.register_attack_callback( RESULT_HIT_MASK, new weapon_stat_proc_callback_t( p, w, buff, 1.0/*PPM*/ ) );
  }
}

void register_mirror_scope( player_t* p, const std::string& enchant, weapon_t* w )
{
  if ( enchant == "mirror_scope" )
  {
    //FIXME: Using gnomish x-ray proc and icd for now. CONFIRM.
    stat_buff_t* buff = stat_buff_creator_t( p, "mirror_scope" )
                        .spell( p -> find_spell( 109092 ) )
                        .cd( timespan_t::from_seconds( 40 ) )
                        .activated( false );

    p -> callbacks.register_attack_callback( RESULT_HIT_MASK, new weapon_stat_proc_callback_t( p, w, buff, 1.0/*PPM*/ ) );
  }
}

void register_elemental_force( player_t* p, const std::string& mh_enchant, const std::string& oh_enchant, weapon_t* mhw, weapon_t* ohw )
{
  if ( mh_enchant == "elemental_force" || oh_enchant == "elemental_force" )
  {
    if ( mh_enchant == "elemental_force" )
    {
      action_callback_t* cb = new weapon_discharge_proc_callback_t( "elemental_force_mh", p, mhw, 1, SCHOOL_ELEMENTAL, 3000, 0, 3.0/*PPM*/, timespan_t::from_seconds( 0.01 )/*CD*/ );
      p -> callbacks.register_attack_callback( RESULT_HIT_MASK, cb );
    }
    if ( oh_enchant == "elemental_force" )
    {
      action_callback_t* cb = new weapon_discharge_proc_callback_t( "elemental_force_oh", p, ohw, 1, SCHOOL_ELEMENTAL, 3000, 0, 3.0/*PPM*/, timespan_t::from_seconds( 0.01 )/*CD*/ );
      p -> callbacks.register_attack_callback( RESULT_HIT_MASK, cb );
    }
    // TO-DO: Confirm proc rate.
    action_callback_t* cb = new weapon_discharge_proc_callback_t( "elemental_force_s", p, 0, 1, SCHOOL_ELEMENTAL, 3000, 0.08/*FIXED*/, 0, timespan_t::from_seconds( 0.01 )/*CD*/ );
    p -> callbacks.register_tick_damage_callback( RESULT_HIT_MASK, cb );
    p -> callbacks.register_direct_damage_callback( RESULT_HIT_MASK, cb );
  }
}

} // END ANONYMOUS NAMESPACE

// ==========================================================================
// Enchant
// ==========================================================================

// enchant::init ============================================================

void enchant::init( player_t* p )
{
  if ( p -> is_pet() ) return;

  // Special Weapn Enchants
  std::string& mh_enchant     = p -> items[ SLOT_MAIN_HAND ].encoded_enchant_str;
  std::string& oh_enchant     = p -> items[ SLOT_OFF_HAND  ].encoded_enchant_str;

  weapon_t* mhw = &( p -> main_hand_weapon );
  weapon_t* ohw = &( p -> off_hand_weapon );

  register_avalanche( p, mh_enchant, oh_enchant, mhw, ohw );

  register_elemental_force( p, mh_enchant, oh_enchant, mhw, ohw );

  register_executioner( p, mh_enchant, oh_enchant, mhw, ohw );

  register_hurricane( p, mh_enchant, oh_enchant, mhw, ohw );

  register_berserking( p, mh_enchant, mhw, "" );
  register_berserking( p, oh_enchant, ohw, "_oh" );

  register_landslide( p, mh_enchant, mhw, "" );
  register_landslide( p, oh_enchant, ohw, "_oh" );

  register_mongoose( p, mh_enchant, mhw, "" );
  register_mongoose( p, oh_enchant, ohw, "_oh" );

  register_power_torrent( p, mh_enchant, "" );
  register_power_torrent( p, oh_enchant, "_oh" );

  register_jade_spirit( p, mh_enchant, "" );
  register_jade_spirit( p, oh_enchant, "_oh" );

  register_windwalk( p, mh_enchant, mhw, "" );
  register_windwalk( p, oh_enchant, ohw, "_oh" );

  register_gnomish_xray( p, mh_enchant, mhw );
  register_lord_blastingtons_scope_of_doom( p, mh_enchant, mhw );
  register_mirror_scope( p, mh_enchant, mhw );

  // Special Meta Gem "Enchants"
  if ( p -> meta_gem == META_THUNDERING_SKYFIRE )
  {
    //FIXME: 0.2 ppm and 40 second icd seems to roughly match in-game behavior, but we need to verify the exact mechanics
    stat_buff_t* buff = stat_buff_creator_t( p, "skyfire_swiftness" )
                        .spell( p -> find_spell( 39959 ) )
                        .cd( timespan_t::from_seconds( 40 ) )
                        .activated( false );
    p -> callbacks.register_attack_callback( RESULT_HIT_MASK, new weapon_stat_proc_callback_t( p, mhw, buff, 0.2/*PPM*/ ) );
    p -> callbacks.register_attack_callback( RESULT_HIT_MASK, new weapon_stat_proc_callback_t( p, ohw, buff, 0.2/*PPM*/ ) );
  }
  if ( p -> meta_gem == META_THUNDERING_SKYFLARE )
  {
    stat_buff_t* buff = stat_buff_creator_t( p, "skyflare_swiftness" )
                        .spell( p -> find_spell( 55379 ) )
                        .cd( timespan_t::from_seconds( 40 ) )
                        .activated( false );
    //FIXME: 0.2 ppm and 40 second icd seems to roughly match in-game behavior, but we need to verify the exact mechanics
    p -> callbacks.register_attack_callback( RESULT_HIT_MASK, new weapon_stat_proc_callback_t( p, mhw, buff, 0.2/*PPM*/ ) );
    p -> callbacks.register_attack_callback( RESULT_HIT_MASK, new weapon_stat_proc_callback_t( p, ohw, buff, 0.2/*PPM*/ ) );
  }

  // Special Item Enchants
  for ( size_t i = 0; i < p -> items.size(); i++ )
  {
    item_t& item = p -> items[ i ];

    if ( item.enchant.stat && item.enchant.school )
    {
      unique_gear::register_stat_discharge_proc( item, item.enchant );
    }
    else if ( item.enchant.stat )
    {
      unique_gear::register_stat_proc( item, item.enchant );
    }
    else if ( item.enchant.school )
    {
      unique_gear::register_discharge_proc( item, item.enchant );
    }
    else if ( item.encoded_enchant_str == "synapse_springs" )
    {
      register_synapse_springs( &item );
      item.unique_enchant = true;
    }
    else if ( item.encoded_addon_str == "synapse_springs" )
    {
      register_synapse_springs( &item );
      item.unique_addon = true;
    }
    else if ( item.encoded_enchant_str == "synapse_springs_2" )
    {
      register_synapse_springs_2( &item );
      item.unique_enchant = true;
    }
    else if ( item.encoded_addon_str == "synapse_springs_2" )
    {
      register_synapse_springs_2( &item );
      item.unique_addon = true;
    }
    else if ( item.encoded_enchant_str == "phase_fingers" )
    {
      register_phase_fingers( &item );
      item.unique_enchant = true;
    }
    else if ( item.encoded_addon_str == "phase_fingers" )
    {
      register_phase_fingers( &item );
      item.unique_addon = true;
    }
  }
}

// enchant::get_reforge_encoding ============================================

bool enchant::get_reforge_encoding( std::string& name,
                                    std::string& encoding,
                                    const std::string& reforge_id )
{
  name.clear();
  encoding.clear();

  if ( reforge_id.empty() || reforge_id == "0" )
    return true;

  int start = 0;
  int target = atoi( reforge_id.c_str() );
  target %= 56;
  if ( target == 0 ) target = 56;
  else if ( target <= start ) return false;

  for ( int i=0; reforge_stats[ i ] != STAT_NONE; i++ )
  {
    for ( int j=0; reforge_stats[ j ] != STAT_NONE; j++ )
    {
      if ( i == j ) continue;
      if ( ++start == target )
      {
        std::string source_stat = util::stat_type_abbrev( reforge_stats[ i ] );
        std::string target_stat = util::stat_type_abbrev( reforge_stats[ j ] );

        name += "Reforge " + source_stat + " to " + target_stat;
        encoding = source_stat + "_" + target_stat;

        return true;
      }
    }
  }

  return false;
}

// enchant::get_reforge_id ==================================================

int enchant::get_reforge_id( stat_e stat_from,
                             stat_e stat_to )
{
  int index_from;
  for ( index_from=0; reforge_stats[ index_from ] != STAT_NONE; index_from++ )
    if ( reforge_stats[ index_from ] == stat_from )
      break;

  int index_to;
  for ( index_to=0; reforge_stats[ index_to ] != STAT_NONE; index_to++ )
    if ( reforge_stats[ index_to ] == stat_to )
      break;

  int id=0;
  for ( int i=0; reforge_stats[ i ] != STAT_NONE; i++ )
  {
    for ( int j=0; reforge_stats[ j ] != STAT_NONE; j++ )
    {
      if ( i == j ) continue;
      id++;
      if ( index_from == i &&
           index_to   == j )
      {
        return id;
      }
    }
  }

  return 0;
}

// enchant::download_reforge ================================================

bool enchant::download_reforge( item_t&            item,
                                const std::string& reforge_id )
{
  item.armory_reforge_str.clear();

  if ( reforge_id.empty() || reforge_id == "0" )
    return true;

  std::string description;
  if ( get_reforge_encoding( description, item.armory_reforge_str, reforge_id ) )
  {
    util::tokenize( item.armory_reforge_str );
    return true;
  }

  return false;
}

// enchant::download_rsuffix ================================================

bool enchant::download_rsuffix( item_t&            item,
                                const std::string& rsuffix_id )
{
  item.armory_random_suffix_str = rsuffix_id;
  return true;
}
