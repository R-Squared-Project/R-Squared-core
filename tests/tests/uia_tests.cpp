/*
 * Copyright (c) 2015-2018 Cryptonomex, Inc., and contributors.
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

#include <boost/test/unit_test.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>

#include <fc/crypto/digest.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE( uia_tests, database_fixture )

BOOST_AUTO_TEST_CASE( create_advanced_uia )
{
   try {
      asset_id_type test_asset_id = db.get_index<asset_object>().get_next_id();
      auto nathan = create_account("nathan");
      asset_create_operation creator;
      creator.issuer = nathan.get_id();
      creator.fee = asset();
      creator.symbol = "ADVANCED";
      creator.common_options.max_supply = 100000000;
      creator.precision = 2;
      creator.common_options.market_fee_percent = GRAPHENE_MAX_MARKET_FEE_PERCENT/100; /*1%*/
      creator.common_options.issuer_permissions = charge_market_fee|white_list|override_authority|transfer_restricted|disable_confidential;
      creator.common_options.flags = charge_market_fee|white_list|override_authority|disable_confidential;
      creator.common_options.core_exchange_rate = price(asset(2),asset(1,asset_id_type(1)));
      creator.common_options.whitelist_authorities = creator.common_options.blacklist_authorities = {account_id_type()};

      trx.operations.push_back(std::move(creator));
      PUSH_TX( db, trx, ~0 );

      const asset_object& test_asset = test_asset_id(db);
      BOOST_CHECK(test_asset.symbol == "ADVANCED");
      BOOST_CHECK(asset(1, test_asset_id) * test_asset.options.core_exchange_rate == asset(2));
      BOOST_CHECK(test_asset.options.flags & white_list);
      BOOST_CHECK(test_asset.options.max_supply == 100000000);
      BOOST_CHECK(!test_asset.bitasset_data_id.valid());
      BOOST_CHECK(test_asset.options.market_fee_percent == GRAPHENE_MAX_MARKET_FEE_PERCENT/100);

      const asset_dynamic_data_object& test_asset_dynamic_data = test_asset.dynamic_asset_data_id(db);
      BOOST_CHECK(test_asset_dynamic_data.current_supply == 0);
      BOOST_CHECK(test_asset_dynamic_data.accumulated_fees == 0);
      BOOST_CHECK(test_asset_dynamic_data.fee_pool == 0);

   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( override_transfer_test )
{ try {
   ACTORS( (dan)(eric)(nathan) );
   const asset_object& advanced = create_user_issued_asset( "ADVANCED", nathan, override_authority );
   BOOST_TEST_MESSAGE( "Issuing 1000 ADVANCED to dan" );
   issue_uia( dan, advanced.amount( 1000 ) );
   BOOST_TEST_MESSAGE( "Checking dan's balance" );
   BOOST_REQUIRE_EQUAL( get_balance( dan, advanced ), 1000 );

   override_transfer_operation otrans;
   otrans.issuer = advanced.issuer;
   otrans.from = dan.id;
   otrans.to   = eric.id;
   otrans.amount = advanced.amount(100);
   trx.operations.clear();
   trx.operations.push_back(otrans);

   BOOST_TEST_MESSAGE( "Require throwing without signature" );
   GRAPHENE_REQUIRE_THROW( PUSH_TX( db, trx, 0 ), tx_missing_active_auth );
   BOOST_TEST_MESSAGE( "Require throwing with dan's signature" );
   sign( trx,  dan_private_key  );
   GRAPHENE_REQUIRE_THROW( PUSH_TX( db, trx, 0 ), tx_missing_active_auth );
   BOOST_TEST_MESSAGE( "Pass with issuer's signature" );
   trx.clear_signatures();
   sign( trx,  nathan_private_key  );
   PUSH_TX( db, trx, 0 );

   BOOST_REQUIRE_EQUAL( get_balance( dan, advanced ), 900 );
   BOOST_REQUIRE_EQUAL( get_balance( eric, advanced ), 100 );
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( override_transfer_test2 )
{ try {
   ACTORS( (dan)(eric)(nathan) );
   const asset_object& advanced = create_user_issued_asset( "ADVANCED", nathan, 0 );
   issue_uia( dan, advanced.amount( 1000 ) );
   BOOST_REQUIRE_EQUAL( get_balance( dan, advanced ), 1000 );

   trx.operations.clear();
   override_transfer_operation otrans;
   otrans.issuer = advanced.issuer;
   otrans.from = dan.id;
   otrans.to   = eric.id;
   otrans.amount = advanced.amount(100);
   trx.operations.push_back(otrans);

   BOOST_TEST_MESSAGE( "Require throwing without signature" );
   GRAPHENE_REQUIRE_THROW( PUSH_TX( db, trx, 0 ), fc::exception);
   BOOST_TEST_MESSAGE( "Require throwing with dan's signature" );
   sign( trx,  dan_private_key  );
   GRAPHENE_REQUIRE_THROW( PUSH_TX( db, trx, 0 ), fc::exception);
   BOOST_TEST_MESSAGE( "Fail because overide_authority flag is not set" );
   trx.clear_signatures();
   sign( trx,  nathan_private_key  );
   GRAPHENE_REQUIRE_THROW( PUSH_TX( db, trx, 0 ), fc::exception );

   BOOST_REQUIRE_EQUAL( get_balance( dan, advanced ), 1000 );
   BOOST_REQUIRE_EQUAL( get_balance( eric, advanced ), 0 );
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( override_transfer_whitelist_test )
{ try {
   ACTORS( (dan)(eric)(nathan) );
   const asset_object& advanced = create_user_issued_asset( "ADVANCED", nathan, white_list | override_authority );
   asset_id_type advanced_id = advanced.id;
   BOOST_TEST_MESSAGE( "Issuing 1000 ADVANCED to dan" );
   issue_uia( dan, advanced.amount( 1000 ) );
   BOOST_TEST_MESSAGE( "Checking dan's balance" );
   BOOST_REQUIRE_EQUAL( get_balance( dan, advanced ), 1000 );

   override_transfer_operation otrans;
   otrans.issuer = advanced.issuer;
   otrans.from = dan.id;
   otrans.to   = eric.id;
   otrans.amount = advanced.amount(100);
   trx.operations.clear();
   trx.operations.push_back(otrans);

   PUSH_TX( db, trx, ~0 );

   BOOST_REQUIRE_EQUAL( get_balance( dan, advanced ), 900 );
   BOOST_REQUIRE_EQUAL( get_balance( eric, advanced ), 100 );

   // Make a whitelist, now it should fail
   {
      BOOST_TEST_MESSAGE( "Changing the whitelist authority" );
      asset_update_operation uop;
      uop.issuer = advanced_id(db).issuer;
      uop.asset_to_update = advanced_id;
      uop.new_options = advanced_id(db).options;
      // The whitelist is managed by dan
      uop.new_options.whitelist_authorities.insert(dan_id);
      trx.operations.clear();
      trx.operations.push_back(uop);
      PUSH_TX( db, trx, ~0 );
      auto whitelist_auths = advanced_id(db).options.whitelist_authorities;
      BOOST_CHECK( whitelist_auths.find(dan_id) != whitelist_auths.end() );

      // Upgrade dan so that he can manage the whitelist
      upgrade_to_lifetime_member( dan_id );

      // Add eric to the whitelist, but do not add dan
      account_whitelist_operation wop;
      wop.authorizing_account = dan_id;
      wop.account_to_list = eric_id;
      wop.new_listing = account_whitelist_operation::white_listed;
      trx.operations.clear();
      trx.operations.push_back(wop);
      PUSH_TX( db, trx, ~0 );
   }

   // Fail because there is a whitelist authority and dan is not whitelisted
   trx.operations.clear();
   trx.operations.push_back(otrans);
   // Now it's able to override-transfer from dan to eric
   PUSH_TX( db, trx, ~0 );

   // Check new balances
   BOOST_REQUIRE_EQUAL( get_balance( dan_id, advanced_id ), 800 );
   BOOST_REQUIRE_EQUAL( get_balance( eric_id, advanced_id ), 200 );

   // Still can not override-transfer to nathan because he is not whitelisted
   otrans.to = nathan_id;
   trx.operations.clear();
   trx.operations.push_back(otrans);
   GRAPHENE_REQUIRE_THROW( PUSH_TX( db, trx, ~0 ), fc::exception );

   generate_block();

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( issue_whitelist_uia )
{
   try {
      account_id_type nathan_id = create_account("nathan").id;
      const asset_id_type uia_id = create_user_issued_asset(
         "ADVANCED", nathan_id(db), white_list ).id;
      account_id_type izzy_id = create_account("izzy").id;
      account_id_type vikram_id = create_account("vikram").id;
      trx.clear();

      asset_issue_operation op;
      op.issuer = uia_id(db).issuer;
      op.asset_to_issue = asset(1000, uia_id);
      op.issue_to_account = izzy_id;
      trx.operations.emplace_back(op);
      set_expiration( db, trx );
      PUSH_TX( db, trx, ~0 );

      BOOST_CHECK(is_authorized_asset( db, izzy_id(db), uia_id(db) ));
      BOOST_CHECK_EQUAL(get_balance(izzy_id, uia_id), 1000);

      // committee-account is free as well
      BOOST_CHECK( is_authorized_asset( db, account_id_type()(db), uia_id(db) ) );

      // Make a whitelist, now it should fail
      {
         BOOST_TEST_MESSAGE( "Changing the whitelist authority" );
         asset_update_operation uop;
         uop.issuer = nathan_id;
         uop.asset_to_update = uia_id;
         uop.new_options = uia_id(db).options;
         uop.new_options.whitelist_authorities.insert(nathan_id);
         trx.operations.back() = uop;
         PUSH_TX( db, trx, ~0 );
         BOOST_CHECK( uia_id(db).options.whitelist_authorities.find(nathan_id) != uia_id(db).options.whitelist_authorities.end() );
      }

      // Fail because there is a whitelist authority and I'm not whitelisted
      trx.operations.back() = op;
      GRAPHENE_REQUIRE_THROW( PUSH_TX( db, trx, ~0 ), fc::exception );

      // committee-account is blocked as well
      BOOST_CHECK( is_authorized_asset( db, account_id_type()(db), uia_id(db) ) );

      account_whitelist_operation wop;
      wop.authorizing_account = nathan_id;
      wop.account_to_list = vikram_id;
      wop.new_listing = account_whitelist_operation::white_listed;

      trx.operations.back() = wop;
      // Fail because whitelist function is restricted to members only
      GRAPHENE_REQUIRE_THROW( PUSH_TX( db, trx, ~0 ), fc::exception );
      upgrade_to_lifetime_member( nathan_id );
      trx.operations.clear();
      trx.operations.push_back( wop );
      PUSH_TX( db, trx, ~0 );

      // Still fail after an irrelevant account was added
      trx.operations.back() = op;
      GRAPHENE_REQUIRE_THROW( PUSH_TX( db, trx, ~0 ), fc::exception );

      wop.account_to_list = izzy_id;
      trx.operations.back() = wop;
      PUSH_TX( db, trx, ~0 );
      trx.operations.back() = op;
      BOOST_CHECK_EQUAL(get_balance(izzy_id, uia_id), 1000);
      // Finally succeed when we were whitelisted
      PUSH_TX( db, trx, ~0 );
      BOOST_CHECK_EQUAL(get_balance(izzy_id, uia_id), 2000);

      // committee-account is still blocked
      BOOST_CHECK( is_authorized_asset( db, account_id_type()(db), uia_id(db) ) );
      // nathan is still blocked
      BOOST_CHECK( !is_authorized_asset( db, nathan_id(db), uia_id(db) ) );

   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( transfer_whitelist_uia )
{
   try {
      INVOKE(issue_whitelist_uia);
      const asset_object& advanced = get_asset("ADVANCED");
      const asset_id_type uia_id = advanced.id;
      const account_object& izzy = get_account("izzy");
      const account_object& dan = create_account("dan");
      account_id_type nathan_id = get_account("nathan").id;
      upgrade_to_lifetime_member(dan);
      trx.clear();

      BOOST_TEST_MESSAGE( "Atempting to transfer asset ADVANCED from izzy to dan when dan is not whitelisted, should fail" );
      transfer_operation op;
      op.fee = advanced.amount(0);
      op.from = izzy.id;
      op.to = dan.id;
      op.amount = advanced.amount(100); //({advanced.amount(0), izzy.id, dan.id, advanced.amount(100)});
      trx.operations.push_back(op);
      //Fail because dan is not whitelisted.
      GRAPHENE_REQUIRE_THROW(PUSH_TX( db, trx, ~0 ), transfer_to_account_not_whitelisted );

      BOOST_TEST_MESSAGE( "Adding dan to whitelist for asset ADVANCED" );
      account_whitelist_operation wop;
      wop.authorizing_account = nathan_id;
      wop.account_to_list = dan.id;
      wop.new_listing = account_whitelist_operation::white_listed;
      trx.operations.back() = wop;
      PUSH_TX( db, trx, ~0 );
      BOOST_TEST_MESSAGE( "Attempting to transfer from izzy to dan after whitelisting dan, should succeed" );
      trx.operations.back() = op;
      PUSH_TX( db, trx, ~0 );

      BOOST_CHECK_EQUAL(get_balance(izzy, advanced), 1900);
      BOOST_CHECK_EQUAL(get_balance(dan, advanced), 100);

      BOOST_TEST_MESSAGE( "Attempting to blacklist izzy" );
      {
         BOOST_TEST_MESSAGE( "Changing the blacklist authority" );
         asset_update_operation uop;
         uop.issuer = nathan_id;
         uop.asset_to_update = advanced.id;
         uop.new_options = advanced.options;
         uop.new_options.blacklist_authorities.insert(nathan_id);
         trx.operations.back() = uop;
         PUSH_TX( db, trx, ~0 );
         BOOST_CHECK( advanced.options.blacklist_authorities.find(nathan_id) != advanced.options.blacklist_authorities.end() );
      }

      wop.new_listing |= account_whitelist_operation::black_listed;
      wop.account_to_list = izzy.id;
      trx.operations.back() = wop;
      PUSH_TX( db, trx, ~0 );
      BOOST_CHECK( !(is_authorized_asset( db, izzy, advanced )) );

      BOOST_TEST_MESSAGE( "Attempting to transfer from izzy after blacklisting, should fail" );
      op.amount = advanced.amount(50);
      trx.operations.back() = op;
      // it fails because the fees are not in a whitelisted asset
      GRAPHENE_REQUIRE_THROW(PUSH_TX( db, trx, ~0 ), fc::exception );

      BOOST_TEST_MESSAGE( "Attempting to burn from izzy after blacklisting, should fail" );
      asset_reserve_operation burn;
      burn.payer = izzy.id;
      burn.amount_to_reserve = advanced.amount(10);
      trx.operations.back() = burn;
      //Fail because izzy is blacklisted
      GRAPHENE_REQUIRE_THROW(PUSH_TX( db, trx, ~0 ), fc::exception);
      BOOST_TEST_MESSAGE( "Attempting transfer from dan back to izzy, should fail because izzy is blacklisted" );
      std::swap(op.from, op.to);
      trx.operations.back() = op;
      //Fail because izzy is blacklisted
      GRAPHENE_REQUIRE_THROW(PUSH_TX( db, trx, ~0 ), fc::exception);

      {
         BOOST_TEST_MESSAGE( "Changing the blacklist authority to dan" );
         asset_update_operation op;
         op.issuer = nathan_id;
         op.asset_to_update = advanced.id;
         op.new_options = advanced.options;
         op.new_options.blacklist_authorities.clear();
         op.new_options.blacklist_authorities.insert(dan.id);
         trx.operations.back() = op;
         PUSH_TX( db, trx, ~0 );
         BOOST_CHECK(advanced.options.blacklist_authorities.find(dan.id) != advanced.options.blacklist_authorities.end());
      }

      BOOST_TEST_MESSAGE( "Attempting to transfer from dan back to izzy" );
      trx.operations.back() = op;
      PUSH_TX( db, trx, ~0 );
      BOOST_CHECK_EQUAL(get_balance(izzy, advanced), 1950);
      BOOST_CHECK_EQUAL(get_balance(dan, advanced), 50);

      BOOST_TEST_MESSAGE( "Blacklisting izzy by dan" );
      wop.authorizing_account = dan.id;
      wop.account_to_list = izzy.id;
      wop.new_listing = account_whitelist_operation::black_listed;
      trx.operations.back() = wop;
      PUSH_TX( db, trx, ~0 );

      trx.operations.back() = op;
      //Fail because izzy is blacklisted
      BOOST_CHECK(!is_authorized_asset( db, izzy, advanced ));
      GRAPHENE_REQUIRE_THROW(PUSH_TX( db, trx, ~0 ), fc::exception);

      //Remove izzy from committee's whitelist, add him to dan's. This should not authorize him to hold ADVANCED.
      wop.authorizing_account = nathan_id;
      wop.account_to_list = izzy.id;
      wop.new_listing = account_whitelist_operation::no_listing;
      trx.operations.back() = wop;
      PUSH_TX( db, trx, ~0 );
      wop.authorizing_account = dan.id;
      wop.account_to_list = izzy.id;
      wop.new_listing = account_whitelist_operation::white_listed;
      trx.operations.back() = wop;
      PUSH_TX( db, trx, ~0 );

      trx.operations.back() = op;
      //Fail because izzy is not whitelisted
      BOOST_CHECK(!is_authorized_asset( db, izzy, advanced ));
      GRAPHENE_REQUIRE_THROW(PUSH_TX( db, trx, ~0 ), fc::exception);

      burn.payer = dan.id;
      burn.amount_to_reserve = advanced.amount(10);
      trx.operations.back() = burn;
      PUSH_TX(db, trx, ~0);
      BOOST_CHECK_EQUAL(get_balance(dan, advanced), 40);

      // committee-account is still blocked
      BOOST_CHECK( is_authorized_asset( db, account_id_type()(db), uia_id(db) ) );
      // nathan is still blocked
      BOOST_CHECK( !is_authorized_asset( db, nathan_id(db), uia_id(db) ) );


      // committee-account is now unblocked
      BOOST_CHECK( is_authorized_asset( db, account_id_type()(db), uia_id(db) ) );
      // nathan is still blocked
      BOOST_CHECK( !is_authorized_asset( db, nathan_id(db), uia_id(db) ) );

   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

/**
 * verify that issuers can halt transfers
 */
BOOST_AUTO_TEST_CASE( transfer_restricted_test )
{
   try
   {
      ACTORS( (nathan)(alice)(bob) );

      BOOST_TEST_MESSAGE( "Issuing 1000 UIA to Alice" );

      auto _issue_uia = [&]( const account_object& recipient, asset amount )
      {
         asset_issue_operation op;
         op.issuer = amount.asset_id(db).issuer;
         op.asset_to_issue = amount;
         op.issue_to_account = recipient.id;
         transaction tx;
         tx.operations.push_back( op );
         set_expiration( db, tx );
         PUSH_TX( db, tx, database::skip_tapos_check | database::skip_transaction_signatures );
      } ;

      const asset_object& uia = create_user_issued_asset( "TXRX", nathan, transfer_restricted );
      _issue_uia( alice, uia.amount( 1000 ) );

      auto _restrict_xfer = [&]( bool xfer_flag )
      {
         asset_update_operation op;
         op.issuer = nathan_id;
         op.asset_to_update = uia.id;
         op.new_options = uia.options;
         if( xfer_flag )
            op.new_options.flags |= transfer_restricted;
         else
            op.new_options.flags &= ~transfer_restricted;
         transaction tx;
         tx.operations.push_back( op );
         set_expiration( db, tx );
         PUSH_TX( db, tx, database::skip_tapos_check | database::skip_transaction_signatures );
      } ;

      BOOST_TEST_MESSAGE( "Enable transfer_restricted, send fails" );

      transfer_operation xfer_op;
      xfer_op.from = alice_id;
      xfer_op.to = bob_id;
      xfer_op.amount = uia.amount(100);
      signed_transaction xfer_tx;
      xfer_tx.operations.push_back( xfer_op );
      set_expiration( db, xfer_tx );
      sign( xfer_tx, alice_private_key );

      _restrict_xfer( true );
      GRAPHENE_REQUIRE_THROW( PUSH_TX( db, xfer_tx ), transfer_restricted_transfer_asset );

      BOOST_TEST_MESSAGE( "Disable transfer_restricted, send succeeds" );

      _restrict_xfer( false );
      PUSH_TX( db, xfer_tx );

      xfer_op.amount = uia.amount(101);

   }
   catch(fc::exception& e)
   {
      edump((e.to_detail_string()));
      throw;
   }
}

/***
 * Test to see if a asset name is valid
 * @param db the database
 * @param acct the account that will attempt to create the asset
 * @param asset_name the asset_name
 * @param allowed whether the creation should be successful
 * @returns true if meets expectations
 */
bool test_asset_name(graphene::chain::database_fixture* db, const graphene::chain::account_object& acct, std::string asset_name, bool allowed)
{
   if (allowed)
   {
      try
      {
         db->create_user_issued_asset(asset_name, acct, 0);
      } catch (...)
      {
         return false;
      }
   }
   else
   {
      try
      {
         db->create_user_issued_asset(asset_name, acct, 0);
         return false;
      } catch (fc::exception& ex) 
      {
         return true;
      } catch (...)
      {
         return false;
      }
   }
   return true;
}

/***
 * Test to see if an ascii character can be used in an asset name
 * @param c the ascii character (NOTE: includes extended ascii up to 255)
 * @param allowed_beginning true if it should be allowed as the first character of an asset name
 * @param allowed_middle true if it should be allowed in the middle of an asset name
 * @param allowed_end true if it should be allowed at the end of an asset name
 * @returns true if tests met expectations
 */
bool test_asset_char(graphene::chain::database_fixture* db, const graphene::chain::account_object& acct, const unsigned char& c, bool allowed_beginning, bool allowed_middle, bool allowed_end)
{
   std::ostringstream asset_name;
   // beginning
   asset_name << c << "CHARLIE";
   if (!test_asset_name(db, acct, asset_name.str(), allowed_beginning))
      return false;

   // middle
   asset_name.str("");
   asset_name.clear();
   asset_name << "CHAR" << c << "LIE";
   if (!test_asset_name(db, acct, asset_name.str(), allowed_middle))
      return false;

   // end
   asset_name.str("");
   asset_name.clear();
   asset_name << "CHARLIE" << c;
   return test_asset_name(db, acct, asset_name.str(), allowed_end);
}

BOOST_AUTO_TEST_CASE( asset_name_test )
{
   try
   {
      ACTORS( (nathan)(bob)(sam) );

      auto has_asset = [&]( std::string symbol ) -> bool
      {
         const auto& assets_by_symbol = db.get_index_type<asset_index>().indices().get<by_symbol>();
         return assets_by_symbol.find( symbol ) != assets_by_symbol.end();
      };

      // Nathan creates asset "ALPHA"
      BOOST_CHECK( !has_asset("ALPHA") );    BOOST_CHECK( !has_asset("ALPHA.ONE") );
      create_user_issued_asset( "ALPHA", nathan_id(db), 0 );
      BOOST_CHECK(  has_asset("ALPHA") );    BOOST_CHECK( !has_asset("ALPHA.ONE") );

      // Nobody can create another asset named ALPHA
      GRAPHENE_REQUIRE_THROW( create_user_issued_asset( "ALPHA",   bob_id(db), 0 ), fc::exception );
      BOOST_CHECK(  has_asset("ALPHA") );    BOOST_CHECK( !has_asset("ALPHA.ONE") );
      GRAPHENE_REQUIRE_THROW( create_user_issued_asset( "ALPHA", nathan_id(db), 0 ), fc::exception );
      BOOST_CHECK(  has_asset("ALPHA") );    BOOST_CHECK( !has_asset("ALPHA.ONE") );


      generate_block();

      // Bob can't create ALPHA.ONE
      GRAPHENE_REQUIRE_THROW( create_user_issued_asset( "ALPHA.ONE", bob_id(db), 0 ), fc::exception );
      BOOST_CHECK(  has_asset("ALPHA") );    BOOST_CHECK( !has_asset("ALPHA.ONE") );

      // Nathan can create ALPHA.ONE
      create_user_issued_asset( "ALPHA.ONE", nathan_id(db), 0 );
      BOOST_CHECK(  has_asset("ALPHA") );    BOOST_CHECK( has_asset("ALPHA.ONE") );

      // create a proposal to create asset ending in a number
      auto& core = asset_id_type()(db);
      asset_create_operation op_p;
      op_p.issuer = nathan_id;
      op_p.symbol = "SP500";
      op_p.common_options.core_exchange_rate = asset( 1 ) / asset( 1, asset_id_type( 1 ) );
      op_p.fee = core.amount(0);

      const auto& curfees = db.get_global_properties().parameters.get_current_fees();
      const auto& proposal_create_fees = curfees.get<proposal_create_operation>();
      proposal_create_operation prop;
      prop.fee_paying_account = nathan_id;
      prop.proposed_ops.emplace_back( op_p );
      prop.expiration_time =  db.head_block_time() + fc::days(1);
      prop.fee = asset( proposal_create_fees.fee + proposal_create_fees.price_per_kbyte );

      signed_transaction tx;
      tx.operations.push_back( prop );
      db.current_fee_schedule().set_fee( tx.operations.back() );
      set_expiration( db, tx );
      sign( tx, nathan_private_key );
      PUSH_TX( db, tx );

      generate_block();

      // Nathan can create asset ending in number
      create_user_issued_asset( "NIKKEI225", nathan_id(db), 0 );
      BOOST_CHECK(  has_asset("NIKKEI225") );

      // make sure other assets can still be created
      create_user_issued_asset( "ALPHA2", nathan_id(db), 0 );
      create_user_issued_asset( "ALPHA2.ONE", nathan_id(db), 0 );
      BOOST_CHECK(  has_asset("ALPHA2") );
      BOOST_CHECK( has_asset("ALPHA2.ONE") );

      // proposal to create asset ending in number will now be created successfully
      prop.expiration_time =  db.head_block_time() + fc::days(3);
      signed_transaction tx_hf620;
      tx_hf620.operations.push_back( prop );
      db.current_fee_schedule().set_fee( tx_hf620.operations.back() );
      set_expiration( db, tx_hf620 );
      sign( tx_hf620, nathan_private_key );
      PUSH_TX( db, tx_hf620 );

      // assets with invalid characters should not be allowed
      unsigned char c = 0;
      do
      {
         if ( (c >= 48 && c <= 57) ) // numbers
            BOOST_CHECK_MESSAGE( test_asset_char(this, nathan_id(db), c, false, true, true), "Failed on good ASCII value " + std::to_string(c) );
         else if ( c >= 65 && c <= 90) // letters
            BOOST_CHECK_MESSAGE( test_asset_char(this, nathan_id(db), c, true, true, true), "Failed on good ASCII value " + std::to_string(c) );
         else                       // everything else
            BOOST_CHECK_MESSAGE( test_asset_char(this, nathan_id(db), c, false, false, false), "Failed on bad ASCII value " + std::to_string(c) );
         c++;
      } while (c != 0);
   }
   catch(fc::exception& e)
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()
