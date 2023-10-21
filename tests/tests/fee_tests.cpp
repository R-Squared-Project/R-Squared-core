/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 * Copyright (c) 2020-2023 Revolution Populi Limited, and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <graphene/chain/hardfork.hpp>

#include <graphene/chain/fba_accumulator_id.hpp>

#include <graphene/chain/fba_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/vesting_balance_object.hpp>
#include <graphene/chain/exceptions.hpp>

#include <fc/uint128.hpp>

#include <boost/test/unit_test.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE( fee_tests, database_fixture )

BOOST_AUTO_TEST_CASE( nonzero_fee_test )
{
   try
   {
      ACTORS((alice)(bob));

      const share_type prec = asset::scaled_precision( asset_id_type()(db).precision );

      // Return number of core shares (times precision)
      auto _core = [&]( int64_t x ) -> asset
      {  return asset( x*prec );    };

      transfer( committee_account, alice_id, _core(1000000) );

      // make sure the database requires our fee to be nonzero
      enable_fees();

      signed_transaction tx;
      transfer_operation xfer_op;
      xfer_op.from = alice_id;
      xfer_op.to = bob_id;
      xfer_op.amount = _core(1000);
      xfer_op.fee = _core(0);
      tx.operations.push_back( xfer_op );
      set_expiration( db, tx );
      sign( tx, alice_private_key );
      GRAPHENE_REQUIRE_THROW( PUSH_TX( db, tx ), insufficient_fee );
   }
   catch( const fc::exception& e )
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(asset_claim_pool_test)
{
    try
    {
        ACTORS((nathan)(bob));
        // Nathan and Bob create some user issued assets
        // Nathan deposits RVP to the fee pool
        // Nathan claimes fee pool of her asset and can't claim pool of Bob's asset

        const share_type core_prec = asset::scaled_precision( asset_id_type()(db).precision );

        // return number of core shares (times precision)
        auto _core = [&core_prec]( int64_t x ) -> asset
        {  return asset( x*core_prec );    };

        const asset_object& nathancoin = create_user_issued_asset( "NATHANCOIN", nathan,  0 );
        const asset_object& nathanusd = create_user_issued_asset( "NATHNAUSD", nathan, 0 );

        asset_id_type nathancoin_id = nathancoin.id;
        asset_id_type nathanusd_id = nathanusd.id;
        asset_id_type bobcoin_id = create_user_issued_asset( "BOBCOIN", nathan, 0).id;

        // prepare users' balance
        issue_uia( nathan, nathanusd.amount( 20000000 ) );
        issue_uia( nathan, nathancoin.amount( 10000000 ) );

        transfer( committee_account, nathan_id, _core(1000) );
        transfer( committee_account, bob_id, _core(1000) );

        enable_fees();

        auto claim_pool = [&]( const account_id_type issuer, const asset_id_type asset_to_claim,
                              const asset& amount_to_fund, const asset_object& fee_asset  )
        {
            asset_claim_pool_operation claim_op;
            claim_op.issuer = issuer;
            claim_op.asset_id = asset_to_claim;
            claim_op.amount_to_claim = amount_to_fund;

            signed_transaction tx;
            tx.operations.push_back( claim_op );
            db.current_fee_schedule().set_fee( tx.operations.back(), fee_asset.options.core_exchange_rate );
            set_expiration( db, tx );
            sign( tx, nathan_private_key );
            PUSH_TX( db, tx );

        };

        auto claim_pool_proposal = [&]( const account_id_type issuer, const asset_id_type asset_to_claim,
                                        const asset& amount_to_fund, const asset_object& fee_asset  )
        {
            asset_claim_pool_operation claim_op;
            claim_op.issuer = issuer;
            claim_op.asset_id = asset_to_claim;
            claim_op.amount_to_claim = amount_to_fund;

            const auto& curfees = db.get_global_properties().parameters.get_current_fees();
            const auto& proposal_create_fees = curfees.get<proposal_create_operation>();
            proposal_create_operation prop;
            prop.fee_paying_account = nathan_id;
            prop.proposed_ops.emplace_back( claim_op );
            prop.expiration_time =  db.head_block_time() + fc::days(1);
            prop.fee = asset( proposal_create_fees.fee + proposal_create_fees.price_per_kbyte );

            signed_transaction tx;
            tx.operations.push_back( prop );
            db.current_fee_schedule().set_fee( tx.operations.back(), fee_asset.options.core_exchange_rate );
            set_expiration( db, tx );
            sign( tx, nathan_private_key );
            PUSH_TX( db, tx );

        };

        // deposit 100 RVP to the fee pool of NATHANUSD asset
        fund_fee_pool( nathan_id(db), nathanusd_id(db), _core(100).amount );

        // New reference for core_asset after having produced blocks
        const asset_object& core_asset_hf = asset_id_type()(db);

        // can't claim pool because it is empty
        GRAPHENE_REQUIRE_THROW( claim_pool( nathan_id, nathancoin_id, _core(1), core_asset_hf), fc::exception );

        // deposit 300 RVP to the fee pool of NATHANCOIN asset
        fund_fee_pool( nathan_id(db), nathancoin_id(db), _core(300).amount );

        // Test amount of CORE in fee pools
        BOOST_CHECK( nathancoin_id(db).dynamic_asset_data_id(db).fee_pool == _core(300).amount );
        BOOST_CHECK( nathanusd_id(db).dynamic_asset_data_id(db).fee_pool == _core(100).amount );

        // can't claim pool of an asset that doesn't belong to you
        GRAPHENE_REQUIRE_THROW( claim_pool( nathan_id, bobcoin_id, _core(200), core_asset_hf), fc::exception );

        // can't claim more than is available in the fee pool
        GRAPHENE_REQUIRE_THROW( claim_pool( nathan_id, nathancoin_id, _core(400), core_asset_hf ), fc::exception );

        // can't pay fee in the same asset whose pool is being drained
        GRAPHENE_REQUIRE_THROW( claim_pool( nathan_id, nathancoin_id, _core(200), nathancoin_id(db) ), fc::exception );

        // can claim RVP back from the fee pool
        claim_pool( nathan_id, nathancoin_id, _core(200), core_asset_hf );
        BOOST_CHECK( nathancoin_id(db).dynamic_asset_data_id(db).fee_pool == _core(100).amount );

        // can pay fee in the asset other than the one whose pool is being drained
        share_type balance_before_claim = get_balance( nathan_id, asset_id_type() );
        claim_pool( nathan_id, nathancoin_id, _core(100), nathanusd_id(db) );
        BOOST_CHECK( nathancoin_id(db).dynamic_asset_data_id(db).fee_pool == _core(0).amount );

        //check balance after claiming pool
        share_type current_balance = get_balance( nathan_id, asset_id_type() );
        BOOST_CHECK( balance_before_claim + _core(100).amount == current_balance );

        // can create a proposal to claim claim pool after hard fork
        claim_pool_proposal( nathan_id, nathanusd_id, _core(1), core_asset_hf);
    }
    FC_LOG_AND_RETHROW()
}

///////////////////////////////////////////////////////////////
// cashback_test infrastructure                              //
///////////////////////////////////////////////////////////////

#define CHECK_BALANCE( actor_name, amount ) \
   BOOST_CHECK_EQUAL( get_balance( actor_name ## _id, asset_id_type() ), amount )

#define CHECK_VESTED_CASHBACK( actor_name, amount ) \
   BOOST_CHECK_EQUAL( actor_name ## _id(db).statistics(db).pending_vested_fees.value, amount )

#define CHECK_UNVESTED_CASHBACK( actor_name, amount ) \
   BOOST_CHECK_EQUAL( actor_name ## _id(db).statistics(db).pending_fees.value, amount )

#define GET_CASHBACK_BALANCE( account ) \
   ( (account.cashback_vb.valid()) \
   ? account.cashback_balance(db).balance.amount.value \
   : 0 )

#define CHECK_CASHBACK_VBO( actor_name, _amount ) \
   BOOST_CHECK_EQUAL( GET_CASHBACK_BALANCE( actor_name ## _id(db) ), _amount )

#define P100 GRAPHENE_100_PERCENT
#define P1 GRAPHENE_1_PERCENT

uint64_t pct( uint64_t percentage, uint64_t val )
{
   fc::uint128_t x = percentage;
   x *= val;
   x /= GRAPHENE_100_PERCENT;
   return static_cast<uint64_t>(x);
}

uint64_t pct( uint64_t percentage0, uint64_t percentage1, uint64_t val )
{
   return pct( percentage1, pct( percentage0, val ) );
}

uint64_t pct( uint64_t percentage0, uint64_t percentage1, uint64_t percentage2, uint64_t val )
{
   return pct( percentage2, pct( percentage1, pct( percentage0, val ) ) );
}

struct actor_audit
{
   int64_t b0 = 0;      // starting balance parameter
   int64_t bal = 0;     // balance should be this
   int64_t ubal = 0;    // unvested balance (in VBO) should be this
   int64_t ucb = 0;     // unvested cashback in account_statistics should be this
   int64_t vcb = 0;     // vested cashback in account_statistics should be this
   int64_t ref_pct = 0; // referrer percentage should be this
};

BOOST_AUTO_TEST_CASE( cashback_test )
{ try {
   /*                        Account Structure used in this test                         *
    *                                                                                    *
    *               /-----------------\       /-------------------\                      *
    *               | life (Lifetime) |       |  rog (Lifetime)   |                      *
    *               \-----------------/       \-------------------/                      *
    *                  | Ref&Reg    | Refers     | Registers  | Registers                *
    *                  |            | 75         | 25         |                          *
    *                  v            v            v            |                          *
    *  /----------------\         /----------------\          |                          *
    *  |  ann (Annual)  |         |  dumy (basic)  |          |                          *
    *  \----------------/         \----------------/          |-------------.            *
    * 80 | Refers      L--------------------------------.     |             |            *
    *    v                     Refers                80 v     v 20          |            *
    *  /----------------\                         /----------------\        |            *
    *  |  scud (basic)  |<------------------------|  stud (basic)  |        |            *
    *  \----------------/ 20   Registers          | (Upgrades to   |        | 5          *
    *                                             |   Lifetime)    |        v            *
    *                                             \----------------/   /--------------\  *
    *                                                         L------->| pleb (Basic) |  *
    *                                                       95 Refers  \--------------/  *
    *                                                                                    *
    * Fee distribution chains (80-20 referral/net split, 50-30 referrer/LTM split)       *
    * life : 80% -> life, 20% -> net                                                     *
    * rog: 80% -> rog, 20% -> net                                                        *
    * ann (before upg): 80% -> life, 20% -> net                                          *
    * ann (after upg): 80% * 5/8 -> ann, 80% * 3/8 -> life, 20% -> net                   *
    * stud (before upg): 80% * 5/8 -> ann, 80% * 3/8 -> life, 20% * 80% -> rog,          *
    *                    20% -> net                                                      *
    * stud (after upg): 80% -> stud, 20% -> net                                          *
    * dumy : 75% * 80% -> life, 25% * 80% -> rog, 20% -> net                             *
    * scud : 80% * 5/8 -> ann, 80% * 3/8 -> life, 20% * 80% -> stud, 20% -> net          *
    * pleb : 95% * 80% -> stud, 5% * 80% -> rog, 20% -> net                              *
    */

   BOOST_TEST_MESSAGE("Creating actors");

   ACTOR(life);
   ACTOR(rog);
   PREP_ACTOR(ann);
   PREP_ACTOR(scud);
   PREP_ACTOR(dumy);
   PREP_ACTOR(stud);
   PREP_ACTOR(pleb);
   // use ##_public_key vars to silence unused variable warning
   BOOST_CHECK_GT(ann_public_key.key_data.size(), 0u);
   BOOST_CHECK_GT(scud_public_key.key_data.size(), 0u);
   BOOST_CHECK_GT(dumy_public_key.key_data.size(), 0u);
   BOOST_CHECK_GT(stud_public_key.key_data.size(), 0u);
   BOOST_CHECK_GT(pleb_public_key.key_data.size(), 0u);

   account_id_type ann_id, scud_id, dumy_id, stud_id, pleb_id;
   actor_audit alife, arog, aann, ascud, adumy, astud, apleb;

   alife.b0 = 100000000;
   arog.b0 = 100000000;
   aann.b0 = 1000000;
   astud.b0 = 1000000;
   astud.ref_pct = 80 * GRAPHENE_1_PERCENT;
   ascud.ref_pct = 80 * GRAPHENE_1_PERCENT;
   adumy.ref_pct = 75 * GRAPHENE_1_PERCENT;
   apleb.ref_pct = 95 * GRAPHENE_1_PERCENT;

   transfer(account_id_type(), life_id, asset(alife.b0));
   alife.bal += alife.b0;
   transfer(account_id_type(), rog_id, asset(arog.b0));
   arog.bal += arog.b0;
   upgrade_to_lifetime_member(life_id);
   upgrade_to_lifetime_member(rog_id);

   BOOST_TEST_MESSAGE("Enable fees");
   const auto& fees = db.get_global_properties().parameters.get_current_fees();

#define CustomRegisterActor(actor_name, registrar_name, referrer_name, referrer_rate) \
   { \
      account_create_operation op; \
      op.registrar = registrar_name ## _id; \
      op.referrer = referrer_name ## _id; \
      op.referrer_percent = referrer_rate*GRAPHENE_1_PERCENT; \
      op.name = BOOST_PP_STRINGIZE(actor_name); \
      op.options.memo_key = actor_name ## _private_key.get_public_key(); \
      op.active = authority(1, public_key_type(actor_name ## _private_key.get_public_key()), 1); \
      op.owner = op.active; \
      op.fee = fees.calculate_fee(op); \
      trx.operations = {op}; \
      sign( trx,  registrar_name ## _private_key ); \
      actor_name ## _id = PUSH_TX( db, trx ).operation_results.front().get<object_id_type>(); \
      trx.clear(); \
   }
#define CustomAuditActor(actor_name)                                \
   if( actor_name ## _id != account_id_type() )                     \
   {                                                                \
      CHECK_BALANCE( actor_name, a ## actor_name.bal );             \
      CHECK_VESTED_CASHBACK( actor_name, a ## actor_name.vcb );     \
      CHECK_UNVESTED_CASHBACK( actor_name, a ## actor_name.ucb );   \
      CHECK_CASHBACK_VBO( actor_name, a ## actor_name.ubal );       \
   }

#define CustomAudit()                                \
   {                                                 \
      CustomAuditActor( life );                      \
      CustomAuditActor( rog );                       \
      CustomAuditActor( ann );                       \
      CustomAuditActor( stud );                      \
      CustomAuditActor( dumy );                      \
      CustomAuditActor( scud );                      \
      CustomAuditActor( pleb );                      \
   }

   int64_t reg_fee    = fees.get< account_create_operation >().premium_fee;
   int64_t xfer_fee   = fees.get< transfer_operation >().fee;
   int64_t upg_an_fee = fees.get< account_upgrade_operation >().membership_annual_fee;
   int64_t upg_lt_fee = fees.get< account_upgrade_operation >().membership_lifetime_fee;
   // all percentages here are cut from whole pie!
   uint64_t network_pct = 20 * P1;
   uint64_t lt_pct = 375 * P100 / 1000;

   BOOST_TEST_MESSAGE("Register and upgrade Ann");
   {
      CustomRegisterActor(ann, life, life, 75);
      alife.vcb += reg_fee; alife.bal += -reg_fee;
      CustomAudit();

      transfer(life_id, ann_id, asset(aann.b0));
      alife.vcb += xfer_fee; alife.bal += -xfer_fee -aann.b0; aann.bal += aann.b0;
      CustomAudit();

      //upgrade_to_annual_member(ann_id);
      upgrade_to_lifetime_member(ann_id);
      aann.ucb += upg_an_fee; aann.bal += -upg_an_fee;

      // audit distribution of fees from Ann
      alife.ubal += pct( P100-network_pct, aann.ucb );
      alife.bal  += pct( P100-network_pct, aann.vcb );
      aann.ucb = 0; aann.vcb = 0;
      CustomAudit();
   }

   BOOST_TEST_MESSAGE("Register dumy and stud");
   CustomRegisterActor(dumy, rog, life, 75);
   arog.vcb += reg_fee; arog.bal += -reg_fee;
   CustomAudit();

   CustomRegisterActor(stud, rog, ann, 80);
   arog.vcb += reg_fee; arog.bal += -reg_fee;
   CustomAudit();

   BOOST_TEST_MESSAGE("Upgrade stud to lifetime member");

   transfer(life_id, stud_id, asset(astud.b0));
   alife.vcb += xfer_fee; alife.bal += -astud.b0 -xfer_fee; astud.bal += astud.b0;
   CustomAudit();

   upgrade_to_lifetime_member(stud_id);
   astud.ucb += upg_lt_fee; astud.bal -= upg_lt_fee;

/*
network_cut:   20000
referrer_cut:  40000 -> ann
registrar_cut: 10000 -> rog
lifetime_cut:  30000 -> life

NET : net
LTM : net' ltm
REF : net' ltm' ref
REG : net' ltm' ref'
*/

   // audit distribution of fees from stud
   alife.ubal += pct( P100-network_pct,      lt_pct,                     astud.ucb );
   aann.ubal  += pct( P100-network_pct, P100-lt_pct,      astud.ref_pct, astud.ucb );
   arog.ubal  += pct( P100-network_pct, P100-lt_pct, P100-astud.ref_pct, astud.ucb );
   astud.ucb  = 0;
   CustomAudit();

   BOOST_TEST_MESSAGE("Register pleb and scud");

   CustomRegisterActor(pleb, rog, stud, 95);
   arog.vcb += reg_fee; arog.bal += -reg_fee;
   CustomAudit();

   CustomRegisterActor(scud, stud, ann, 80);
   astud.vcb += reg_fee; astud.bal += -reg_fee;
   CustomAudit();

   generate_block();

   BOOST_TEST_MESSAGE("Wait for maintenance interval");

   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
   // audit distribution of fees from life
   alife.ubal += pct( P100-network_pct, alife.ucb +alife.vcb );
   alife.ucb = 0; alife.vcb = 0;

   // audit distribution of fees from rog
   arog.ubal += pct( P100-network_pct, arog.ucb + arog.vcb );
   arog.ucb = 0; arog.vcb = 0;

   // audit distribution of fees from ann
   alife.ubal += pct( P100-network_pct,      lt_pct,                    aann.ucb+aann.vcb );
   aann.ubal  += pct( P100-network_pct, P100-lt_pct,      aann.ref_pct, aann.ucb+aann.vcb );
   alife.ubal += pct( P100-network_pct, P100-lt_pct, P100-aann.ref_pct, aann.ucb+aann.vcb );
   aann.ucb = 0; aann.vcb = 0;

   // audit distribution of fees from stud
   astud.ubal += pct( P100-network_pct,                                  astud.ucb+astud.vcb );
   astud.ucb = 0; astud.vcb = 0;

   // audit distribution of fees from dumy
   alife.ubal += pct( P100-network_pct,      lt_pct,                     adumy.ucb+adumy.vcb );
   alife.ubal += pct( P100-network_pct, P100-lt_pct,      adumy.ref_pct, adumy.ucb+adumy.vcb );
   arog.ubal  += pct( P100-network_pct, P100-lt_pct, P100-adumy.ref_pct, adumy.ucb+adumy.vcb );
   adumy.ucb = 0; adumy.vcb = 0;

   // audit distribution of fees from scud
   alife.ubal += pct( P100-network_pct,      lt_pct,                     ascud.ucb+ascud.vcb );
   aann.ubal  += pct( P100-network_pct, P100-lt_pct,      ascud.ref_pct, ascud.ucb+ascud.vcb );
   astud.ubal += pct( P100-network_pct, P100-lt_pct, P100-ascud.ref_pct, ascud.ucb+ascud.vcb );
   ascud.ucb = 0; ascud.vcb = 0;

   // audit distribution of fees from pleb
   astud.ubal += pct( P100-network_pct,      lt_pct,                     apleb.ucb+apleb.vcb );
   astud.ubal += pct( P100-network_pct, P100-lt_pct,      apleb.ref_pct, apleb.ucb+apleb.vcb );
   arog.ubal  += pct( P100-network_pct, P100-lt_pct, P100-apleb.ref_pct, apleb.ucb+apleb.vcb );
   apleb.ucb = 0; apleb.vcb = 0;

   CustomAudit();

   BOOST_TEST_MESSAGE("Doing some transfers");

   transfer(stud_id, scud_id, asset(500000));
   astud.bal += -500000-xfer_fee; astud.vcb += xfer_fee; ascud.bal += 500000;
   CustomAudit();

   transfer(scud_id, pleb_id, asset(400000));
   ascud.bal += -400000-xfer_fee; ascud.vcb += xfer_fee; apleb.bal += 400000;
   CustomAudit();

   transfer(pleb_id, dumy_id, asset(300000));
   apleb.bal += -300000-xfer_fee; apleb.vcb += xfer_fee; adumy.bal += 300000;
   CustomAudit();

   transfer(dumy_id, rog_id, asset(200000));
   adumy.bal += -200000-xfer_fee; adumy.vcb += xfer_fee; arog.bal += 200000;
   CustomAudit();

   BOOST_TEST_MESSAGE("Waiting for maintenance time");

   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

   // audit distribution of fees from life
   alife.ubal += pct( P100-network_pct, alife.ucb +alife.vcb );
   alife.ucb = 0; alife.vcb = 0;

   // audit distribution of fees from rog
   arog.ubal += pct( P100-network_pct, arog.ucb + arog.vcb );
   arog.ucb = 0; arog.vcb = 0;

   // audit distribution of fees from ann
   alife.ubal += pct( P100-network_pct,      lt_pct,                    aann.ucb+aann.vcb );
   aann.ubal  += pct( P100-network_pct, P100-lt_pct,      aann.ref_pct, aann.ucb+aann.vcb );
   alife.ubal += pct( P100-network_pct, P100-lt_pct, P100-aann.ref_pct, aann.ucb+aann.vcb );
   aann.ucb = 0; aann.vcb = 0;

   // audit distribution of fees from stud
   astud.ubal += pct( P100-network_pct,                                  astud.ucb+astud.vcb );
   astud.ucb = 0; astud.vcb = 0;

   // audit distribution of fees from dumy
   alife.ubal += pct( P100-network_pct,      lt_pct,                     adumy.ucb+adumy.vcb );
   alife.ubal += pct( P100-network_pct, P100-lt_pct,      adumy.ref_pct, adumy.ucb+adumy.vcb );
   arog.ubal  += pct( P100-network_pct, P100-lt_pct, P100-adumy.ref_pct, adumy.ucb+adumy.vcb );
   adumy.ucb = 0; adumy.vcb = 0;

   // audit distribution of fees from scud
   alife.ubal += pct( P100-network_pct,      lt_pct,                     ascud.ucb+ascud.vcb );
   aann.ubal  += pct( P100-network_pct, P100-lt_pct,      ascud.ref_pct, ascud.ucb+ascud.vcb );
   astud.ubal += pct( P100-network_pct, P100-lt_pct, P100-ascud.ref_pct, ascud.ucb+ascud.vcb );
   ascud.ucb = 0; ascud.vcb = 0;

   // audit distribution of fees from pleb
   astud.ubal += pct( P100-network_pct,      lt_pct,                     apleb.ucb+apleb.vcb );
   astud.ubal += pct( P100-network_pct, P100-lt_pct,      apleb.ref_pct, apleb.ucb+apleb.vcb );
   arog.ubal  += pct( P100-network_pct, P100-lt_pct, P100-apleb.ref_pct, apleb.ucb+apleb.vcb );
   apleb.ucb = 0; apleb.vcb = 0;

   CustomAudit();

   BOOST_TEST_MESSAGE("Waiting for annual membership to expire");

   //generate_blocks(ann_id(db).membership_expiration_date);
   generate_block();

   BOOST_TEST_MESSAGE("Transferring from scud to pleb");

   //ann's membership has expired, so scud's fee should go up to life instead.
   transfer(scud_id, pleb_id, asset(10));
   ascud.bal += -10-xfer_fee; ascud.vcb += xfer_fee; apleb.bal += 10;
   CustomAudit();

   BOOST_TEST_MESSAGE("Waiting for maint interval");

   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

   // audit distribution of fees from scud
   alife.ubal += pct( P100-network_pct,      lt_pct,                     ascud.ucb+ascud.vcb );
   alife.ubal += pct( P100-network_pct, P100-lt_pct,      ascud.ref_pct, ascud.ucb+ascud.vcb );
   astud.ubal += pct( P100-network_pct, P100-lt_pct, P100-ascud.ref_pct, ascud.ucb+ascud.vcb );
   ascud.ucb = 0; ascud.vcb = 0;

   CustomAudit();

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( account_create_fee_scaling )
{ try {
   auto accounts_per_scale = db.get_global_properties().parameters.accounts_per_fee_scale;
   db.modify(global_property_id_type()(db), [](global_property_object& gpo)
   {
      gpo.parameters.get_mutable_fees() = fee_schedule::get_default();
      gpo.parameters.get_mutable_fees().get<account_create_operation>().basic_fee = 1;
   });

   for( int i = db.get_dynamic_global_properties().accounts_registered_this_interval; i < accounts_per_scale; ++i )
   {
      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.get_current_fees().get<account_create_operation>().basic_fee, 1u);
      create_account("shill" + fc::to_string(i));
   }
   for( int i = 0; i < accounts_per_scale; ++i )
   {
      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.get_current_fees().get<account_create_operation>().basic_fee, 16u);
      create_account("moreshills" + fc::to_string(i));
   }
   for( int i = 0; i < accounts_per_scale; ++i )
   {
      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.get_current_fees().get<account_create_operation>().basic_fee, 256u);
      create_account("moarshills" + fc::to_string(i));
   }
   BOOST_CHECK_EQUAL(db.get_global_properties().parameters.get_current_fees().get<account_create_operation>().basic_fee, 4096u);

   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
   BOOST_CHECK_EQUAL(db.get_global_properties().parameters.get_current_fees().get<account_create_operation>().basic_fee, 1u);
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( stealth_fba_test )
{
   try
   {
      ACTORS( (alice)(bob)(chloe)(dan)(nathan)(philbin)(tom) );
      upgrade_to_lifetime_member(philbin_id);

      // Philbin (registrar who registers Rex)

      // Nathan (initial issuer of stealth asset, will later transfer to Tom)
      // Alice, Bob, Chloe, Dan (ABCD)
      // Rex (recycler -- buyback account for stealth asset)
      // Tom (owner of stealth asset who will be set as top_n authority)

      // Nathan creates STEALTH
      asset_id_type stealth_id = create_user_issued_asset( "STEALTH", nathan_id(db),
         disable_confidential | transfer_restricted | override_authority | white_list | charge_market_fee ).id;

      /*
      // this is disabled because it doesn't work, our modify() is probably being overwritten by undo

      //
      // Init blockchain with stealth ID's
      // On a real chain, this would be done with #define GRAPHENE_FBA_STEALTH_DESIGNATED_ASSET
      // causing the designated_asset fields of these objects to be set at genesis, but for
      // this test we modify the db directly.
      //
      auto set_fba_asset = [&]( uint64_t fba_acc_id, asset_id_type asset_id )
      {
         db.modify( fba_accumulator_id_type(fba_acc_id)(db), [&]( fba_accumulator_object& fba )
         {
            fba.designated_asset = asset_id;
         } );
      };
      */

      // Nathan kills some permission bits (this somehow happened to the real STEALTH in production)
      {
         asset_update_operation update_op;
         update_op.issuer = nathan_id;
         update_op.asset_to_update = stealth_id;
         asset_options new_options;
         new_options = stealth_id(db).options;
         new_options.issuer_permissions = charge_market_fee;
         new_options.flags = disable_confidential | transfer_restricted | override_authority | white_list | charge_market_fee;
         // after fixing #579 you should be able to delete the following line
         new_options.core_exchange_rate = price( asset( 1, stealth_id ), asset( 1, asset_id_type() ) );
         update_op.new_options = new_options;
         signed_transaction tx;
         tx.operations.push_back( update_op );
         set_expiration( db, tx );
         sign( tx, nathan_private_key );
         PUSH_TX( db, tx );
      }

      // Nathan transfers issuer duty to Tom
/*      {
         asset_update_operation update_op;
         update_op.issuer = nathan_id;
         update_op.asset_to_update = stealth_id;
         //update_op.new_issuer = tom_id;
         // new_options should be optional, but isn't...the following line should be unnecessary #580
         update_op.new_options = stealth_id(db).options;
         signed_transaction tx;
         tx.operations.push_back( update_op );
         set_expiration( db, tx );
         sign( tx, nathan_private_key );
         PUSH_TX( db, tx );
      }
*/
      {
         asset_update_issuer_operation upd_op;
         upd_op.asset_to_update = stealth_id;
         upd_op.issuer = nathan_id;
         upd_op.new_issuer = tom_id;
         signed_transaction tx;
         tx.operations.push_back( upd_op );
         set_expiration( db, tx );
         sign( tx, nathan_private_key );
         PUSH_TX( db, tx );
      }

      // Tom re-enables the permission bits to clear the flags, then clears them again
      // Allowed by #572 when current_supply == 0
      {
         asset_update_operation update_op;
         update_op.issuer = tom_id;
         update_op.asset_to_update = stealth_id;
         asset_options new_options;
         new_options = stealth_id(db).options;
         new_options.issuer_permissions = new_options.flags | charge_market_fee;
         update_op.new_options = new_options;
         signed_transaction tx;
         // enable perms is one op
         tx.operations.push_back( update_op );

         new_options.issuer_permissions = charge_market_fee;
         new_options.flags = charge_market_fee;
         update_op.new_options = new_options;
         // reset wrongly set flags and reset permissions can be done in a single op
         tx.operations.push_back( update_op );

         set_expiration( db, tx );
         sign( tx, tom_private_key );
         PUSH_TX( db, tx );
      }

      // Philbin registers Rex who will be the asset's buyback, including sig from the new issuer (Tom)
      account_id_type rex_id;
      {
         buyback_account_options bbo;
         bbo.asset_to_buy = stealth_id;
         bbo.asset_to_buy_issuer = tom_id;
         bbo.markets.emplace( asset_id_type() );
         account_create_operation create_op = make_account( "rex" );
         create_op.registrar = philbin_id;
         create_op.extensions.value.buyback_options = bbo;
         create_op.owner = authority::null_authority();
         create_op.active = authority::null_authority();

         signed_transaction tx;
         tx.operations.push_back( create_op );
         set_expiration( db, tx );
         sign( tx, philbin_private_key );
         sign( tx, tom_private_key );

         processed_transaction ptx = PUSH_TX( db, tx );
         rex_id = ptx.operation_results.back().get< object_id_type >();
      }

      // Tom issues some asset to Alice and Bob
      set_expiration( db, trx );  // #11
      issue_uia( alice_id, asset( 1000, stealth_id ) );
      issue_uia(   bob_id, asset( 1000, stealth_id ) );

      // Tom sets his authority to the top_n of the asset
      {
         top_holders_special_authority top2;
         top2.num_top_holders = 2;
         top2.asset = stealth_id;

         account_update_operation op;
         op.account = tom_id;
         op.extensions.value.active_special_authority = top2;
         op.extensions.value.owner_special_authority = top2;

         signed_transaction tx;
         tx.operations.push_back( op );

         set_expiration( db, tx );
         sign( tx, tom_private_key );

         PUSH_TX( db, tx );
      }

      // Wait until the next maintenance interval for top_n to take effect
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

      idump( ( get_operation_history( chloe_id ) ) );
      idump( ( get_operation_history( rex_id ) ) );
      idump( ( get_operation_history( tom_id ) ) );
   }
   catch( const fc::exception& e )
   {
      elog( "caught exception ${e}", ("e", e.to_detail_string()) );
      throw;
   }
}

BOOST_AUTO_TEST_CASE( sub_asset_creation_fee_test )
{ try {
   fee_schedule schedule;

   asset_create_operation::fee_parameters_type default_ac_fee;

   asset_create_operation op;
   op.symbol = "TEST.SUB";

   auto op_size = fc::raw::pack_size(op);

   auto expected_data_fee = op.calculate_data_fee( op_size, default_ac_fee.price_per_kbyte );
   int64_t expected_fee = default_ac_fee.long_symbol + expected_data_fee;

   // no fees set yet -> default
   BOOST_TEST_MESSAGE("Testing default fee schedule");
   asset fee = schedule.calculate_fee( op );
   BOOST_CHECK_EQUAL( fee.amount.value, expected_fee );

   // set fee + check
   asset_create_operation::fee_parameters_type ac_fee;
   ac_fee.long_symbol = 100100;
   ac_fee.symbol4 = 2000200;
   ac_fee.symbol3 = 30000300;
   ac_fee.price_per_kbyte = 1050;

   schedule.parameters.insert( ac_fee );

   expected_data_fee = op.calculate_data_fee( op_size, ac_fee.price_per_kbyte );
   expected_fee = ac_fee.long_symbol + expected_data_fee;

   fee = schedule.calculate_fee( op );
   BOOST_CHECK_EQUAL( fee.amount.value, expected_fee );

   // set fee for account_transfer_operation, no change on asset creation fee
   BOOST_TEST_MESSAGE("Testing our fee schedule without sub-asset creation fee enabled");
   account_transfer_operation::fee_parameters_type at_fee;
   at_fee.fee = 5500;

   schedule.parameters.insert( at_fee );

   fee = schedule.calculate_fee( op );
   BOOST_CHECK_EQUAL( fee.amount.value, expected_fee );

   // enable sub-asset creation fee
   BOOST_TEST_MESSAGE("Testing our fee schedule with sub-asset creation fee enabled");
   schedule.parameters.insert( ticket_create_operation::fee_parameters_type() );

   expected_fee = at_fee.fee + expected_data_fee;

   fee = schedule.calculate_fee( op );
   BOOST_CHECK_EQUAL( fee.amount.value, expected_fee );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( issue_429_test )
{
   try
   {
      ACTORS((nathan));

      transfer( committee_account, nathan_id, asset( 1000000 * asset::scaled_precision( asset_id_type()(db).precision ) ) );

      // make sure the database requires our fee to be nonzero
      enable_fees();

      const auto& fees = db.get_global_properties().parameters.get_current_fees();
      auto fees_to_pay = fees.get<asset_create_operation>();

      {
         signed_transaction tx;
         asset_create_operation op;
         op.issuer = nathan_id;
         op.symbol = "NATHAN";
         op.common_options.core_exchange_rate = asset( 1 ) / asset( 1, asset_id_type( 1 ) );
         op.fee = asset( (fees_to_pay.long_symbol + fees_to_pay.price_per_kbyte) & (~1) );
         tx.operations.push_back( op );
         set_expiration( db, tx );
         sign( tx, nathan_private_key );
         PUSH_TX( db, tx );
      }

      verify_asset_supplies( db );

      {
         signed_transaction tx;
         asset_create_operation op;
         op.issuer = nathan_id;
         op.symbol = "NATHAN.ODD";
         op.common_options.core_exchange_rate = asset( 1 ) / asset( 1, asset_id_type( 1 ) );
         op.fee = asset((fees_to_pay.long_symbol + fees_to_pay.price_per_kbyte) | 1);
         tx.operations.push_back( op );
         set_expiration( db, tx );
         sign( tx, nathan_private_key );
         PUSH_TX( db, tx );
      }

      verify_asset_supplies( db );

      {
         signed_transaction tx;
         asset_create_operation op;
         op.issuer = nathan_id;
         op.symbol = "NATHAN.ODDER";
         op.common_options.core_exchange_rate = asset( 1 ) / asset( 1, asset_id_type( 1 ) );
         op.fee = asset((fees_to_pay.long_symbol + fees_to_pay.price_per_kbyte) | 1);
         tx.operations.push_back( op );
         set_expiration( db, tx );
         sign( tx, nathan_private_key );
         PUSH_TX( db, tx );
      }

      verify_asset_supplies( db );
   }
   catch( const fc::exception& e )
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( issue_433_test )
{
   try
   {
      ACTORS((nathan));

      auto& core = asset_id_type()(db);

      transfer( committee_account, nathan_id, asset( 1000000 * asset::scaled_precision( core.precision ) ) );

      const auto& myusd = create_user_issued_asset( "MYUSD", nathan, 0 );
      issue_uia( nathan, myusd.amount( 2000000000 ) );

      // make sure the database requires our fee to be nonzero
      enable_fees();

      const auto& fees = db.get_global_properties().parameters.get_current_fees();
      const auto asset_create_fees = fees.get<asset_create_operation>();

      fund_fee_pool( nathan, myusd, 5*asset_create_fees.long_symbol );

      asset_create_operation op;
      op.issuer = nathan_id;
      op.symbol = "NATHAN";
      op.common_options.core_exchange_rate = asset( 1 ) / asset( 1, asset_id_type( 1 ) );
      op.fee = myusd.amount( ((asset_create_fees.long_symbol + asset_create_fees.price_per_kbyte) & (~1)) );
      signed_transaction tx;
      tx.operations.push_back( op );
      set_expiration( db, tx );
      sign( tx, nathan_private_key );
      PUSH_TX( db, tx );

      verify_asset_supplies( db );
   }
   catch( const fc::exception& e )
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( issue_433_indirect_test )
{
   try
   {
      ACTORS((nathan));

      auto& core = asset_id_type()(db);

      transfer( committee_account, nathan_id, asset( 1000000 * asset::scaled_precision( core.precision ) ) );

      const auto& myusd = create_user_issued_asset( "MYUSD", nathan, 0 );
      issue_uia( nathan, myusd.amount( 2000000000 ) );

      // make sure the database requires our fee to be nonzero
      enable_fees();

      const auto& fees = db.get_global_properties().parameters.get_current_fees();
      const auto asset_create_fees = fees.get<asset_create_operation>();

      fund_fee_pool( nathan, myusd, 5*asset_create_fees.long_symbol );

      asset_create_operation op;
      op.issuer = nathan_id;
      op.symbol = "NATHAN";
      op.common_options.core_exchange_rate = asset( 1 ) / asset( 1, asset_id_type( 1 ) );
      op.fee = myusd.amount( ((asset_create_fees.long_symbol + asset_create_fees.price_per_kbyte) & (~1)) );

      const auto proposal_create_fees = fees.get<proposal_create_operation>();
      proposal_create_operation prop;
      prop.fee_paying_account = nathan_id;
      prop.proposed_ops.emplace_back( op );
      prop.expiration_time =  db.head_block_time() + fc::days(1);
      prop.fee = asset( proposal_create_fees.fee + proposal_create_fees.price_per_kbyte );
      object_id_type proposal_id;
      {
         signed_transaction tx;
         tx.operations.push_back( prop );
         set_expiration( db, tx );
         sign( tx, nathan_private_key );
         proposal_id = PUSH_TX( db, tx ).operation_results.front().get<object_id_type>();
      }
      const proposal_object& proposal = db.get<proposal_object>( proposal_id );

      const auto proposal_update_fees = fees.get<proposal_update_operation>();
      proposal_update_operation pup;
      pup.proposal = proposal.id;
      pup.fee_paying_account = nathan_id;
      pup.active_approvals_to_add.insert(nathan_id);
      pup.fee = asset( proposal_update_fees.fee + proposal_update_fees.price_per_kbyte );
      {
         signed_transaction tx;
         tx.operations.push_back( pup );
         set_expiration( db, tx );
         sign( tx, nathan_private_key );
         PUSH_TX( db, tx );
      }

      verify_asset_supplies( db );
   }
   catch( const fc::exception& e )
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()
