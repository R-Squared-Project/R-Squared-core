/*
 * Copyright (c) 2020 Michel Santos, and contributors.
 * Copyright (c) 2020-2023 Revolution Populi Limited, and contributors.
 * Copyright (c) 2023 R-Squared Labs LLC, and contributors.
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

#include <string>
#include <boost/test/unit_test.hpp>
#include <fc/exception/exception.hpp>

#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/market_object.hpp>

#include "../common/database_fixture.hpp"


using namespace graphene::chain;
using namespace graphene::chain::test;

struct simple_maker_taker_database_fixture : database_fixture {
   simple_maker_taker_database_fixture()
           : database_fixture() {
   }

   const asset_create_operation create_user_issued_asset_operation(const string &name, const account_object &issuer,
                                                                   uint16_t flags, const price &core_exchange_rate,
                                                                   uint8_t precision, uint16_t maker_fee_percent,
                                                                   uint16_t taker_fee_percent) {
      asset_create_operation creator;
      creator.issuer = issuer.id;
      creator.fee = asset();
      creator.symbol = name;
      creator.common_options.max_supply = 0;
      creator.precision = precision;

      creator.common_options.core_exchange_rate = core_exchange_rate;
      creator.common_options.max_supply = GRAPHENE_MAX_SHARE_SUPPLY;
      creator.common_options.flags = flags;
      creator.common_options.issuer_permissions = flags;
      creator.common_options.market_fee_percent = maker_fee_percent;
      creator.common_options.extensions.value.taker_fee_percent = taker_fee_percent;

      return creator;

   }
};


/**
 * BSIP81: Asset owners may specify different market fee rate for maker orders and taker orders
 */
BOOST_FIXTURE_TEST_SUITE(simple_maker_taker_fee_tests, simple_maker_taker_database_fixture)

   /**
    * Test of setting taker fee before HF and after HF for a UIA
    */
   BOOST_AUTO_TEST_CASE(setting_taker_fees_uia) {
      try {
         // Initialize for the current time
         trx.clear();
         set_expiration(db, trx);

         // Initialize actors
         ACTORS((rsquaredchp1)(izzy));
         account_id_type issuer_id = rsquaredchp1.id;
         fc::ecc::private_key issuer_private_key = rsquaredchp1_private_key;

         // Initialize tokens
         price price(asset(1, asset_id_type(1)), asset(1));
         uint16_t market_fee_percent = 20 * GRAPHENE_1_PERCENT;
         const asset_object rsquaredchp1coin = create_user_issued_asset("NCOIN", rsquaredchp1, charge_market_fee, price, 2,
                                                                market_fee_percent);

         //////
         // Test inability to set taker fees
         //////
         asset_update_operation uop;
         uop.issuer = issuer_id;
         uop.asset_to_update = rsquaredchp1coin.get_id();
         uop.new_options = rsquaredchp1coin.options;
         uint16_t new_taker_fee_percent = uop.new_options.market_fee_percent / 2;
         uop.new_options.extensions.value.taker_fee_percent = new_taker_fee_percent;

         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, issuer_private_key);
         PUSH_TX(db, trx);

         // Check the taker fee
         asset_object updated_asset = rsquaredchp1coin.get_id()(db);
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());

         generate_block();
         trx.clear();
         set_expiration(db, trx);


         //////
         // After HF, test default values of taker fee after HF
         // After the HF its default value should still not be set
         //////
         updated_asset = rsquaredchp1coin.get_id()(db);
         uint16_t expected_taker_fee_percent = updated_asset.options.market_fee_percent;
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());


         //////
         // After HF, test invalid taker fees
         //////
         uop.new_options.extensions.value.taker_fee_percent = GRAPHENE_100_PERCENT + 1;
         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, issuer_private_key);
         GRAPHENE_CHECK_THROW(PUSH_TX(db, trx), fc::exception); // An exception should be thrown indicating the reason
         // TODO: Check the specific exception?


         //////
         // After HF, test that new values can be set
         //////
         uop.new_options.extensions.value.taker_fee_percent = new_taker_fee_percent;
         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, issuer_private_key);
         PUSH_TX(db, trx); // No exception should be thrown

         // Check the taker fee
         updated_asset = rsquaredchp1coin.get_id()(db);
         expected_taker_fee_percent = new_taker_fee_percent;
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());
         BOOST_CHECK_EQUAL(expected_taker_fee_percent, *updated_asset.options.extensions.value.taker_fee_percent);


         //////
         // After HF, test ability to set taker fees with an asset update operation inside of a proposal
         //////
         {
            trx.clear();
            set_expiration(db, trx);

            uint64_t alternate_taker_fee_percent = new_taker_fee_percent * 2;
            uop.new_options.extensions.value.taker_fee_percent = alternate_taker_fee_percent;

            proposal_create_operation cop;
            cop.review_period_seconds = 86400;
            uint32_t buffer_seconds = 60 * 60;
            cop.expiration_time = db.head_block_time() + *cop.review_period_seconds + buffer_seconds;
            cop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
            cop.proposed_ops.emplace_back(uop);

            trx.operations.push_back(cop);
            // sign(trx, issuer_private_key);
            processed_transaction processed = PUSH_TX(db, trx); // No exception should be thrown

            // Check the taker fee is not changed because the proposal has not been approved
            updated_asset = rsquaredchp1coin.get_id()(db);
            expected_taker_fee_percent = new_taker_fee_percent;
            BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());
            BOOST_CHECK_EQUAL(expected_taker_fee_percent, *updated_asset.options.extensions.value.taker_fee_percent);


            // Approve the proposal
            trx.clear();
            proposal_id_type pid = processed.operation_results[0].get<object_id_type>();

            proposal_update_operation pup;
            pup.fee_paying_account = rsquaredchp1.id;
            pup.proposal = pid;
            pup.active_approvals_to_add.insert(rsquaredchp1.id);
            trx.operations.push_back(pup);
            set_expiration(db, trx);
            sign(trx, rsquaredchp1_private_key);

            PUSH_TX(db, trx); // No exception should be thrown

            // Advance to after proposal expires
            generate_blocks(cop.expiration_time);

            // Check the taker fee is not changed because the proposal has not been approved
            updated_asset = rsquaredchp1coin.get_id()(db);
            expected_taker_fee_percent = alternate_taker_fee_percent;
            BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());
            BOOST_CHECK_EQUAL(expected_taker_fee_percent, *updated_asset.options.extensions.value.taker_fee_percent);

         }


         //////
         // After HF, test ability to set taker fees with an asset create operation inside of a proposal
         //////
         {
            trx.clear();
            set_expiration(db, trx);

            uint64_t maker_fee_percent = 10 * GRAPHENE_1_PERCENT;
            uint64_t taker_fee_percent = 2 * GRAPHENE_1_PERCENT;
            asset_create_operation ac_op = create_user_issued_asset_operation("NCOIN2", rsquaredchp1, charge_market_fee, price,
                                                                              2,
                                                                              maker_fee_percent, taker_fee_percent);

            proposal_create_operation cop;
            cop.review_period_seconds = 86400;
            uint32_t buffer_seconds = 60 * 60;
            cop.expiration_time = db.head_block_time() + *cop.review_period_seconds + buffer_seconds;
            cop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
            cop.proposed_ops.emplace_back(ac_op);

            trx.operations.push_back(cop);
            // sign(trx, issuer_private_key);

            processed_transaction processed = PUSH_TX(db, trx); // No exception should be thrown

            // Check the asset does not exist because the proposal has not been approved
            const auto& asset_idx = db.get_index_type<asset_index>().indices().get<by_symbol>();
            const auto itr = asset_idx.find("JCOIN2");
            BOOST_CHECK(itr == asset_idx.end());

            // Approve the proposal
            trx.clear();
            proposal_id_type pid = processed.operation_results[0].get<object_id_type>();

            proposal_update_operation pup;
            pup.fee_paying_account = rsquaredchp1.id;
            pup.proposal = pid;
            pup.active_approvals_to_add.insert(rsquaredchp1.id);
            trx.operations.push_back(pup);
            set_expiration(db, trx);
            sign(trx, rsquaredchp1_private_key);

            PUSH_TX(db, trx); // No exception should be thrown

            // Advance to after proposal expires
            generate_blocks(cop.expiration_time);

            // Check the taker fee is not changed because the proposal has not been approved
            BOOST_CHECK(asset_idx.find("NCOIN2") != asset_idx.end());
            updated_asset = *asset_idx.find("NCOIN2");
            expected_taker_fee_percent = taker_fee_percent;
            BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());
            BOOST_CHECK_EQUAL(expected_taker_fee_percent, *updated_asset.options.extensions.value.taker_fee_percent);
            uint16_t expected_maker_fee_percent = maker_fee_percent;
            BOOST_CHECK_EQUAL(expected_maker_fee_percent, updated_asset.options.market_fee_percent);

         }

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Test of setting taker fee before HF and after HF for a smart asset
    */
   BOOST_AUTO_TEST_CASE(setting_taker_fees_smart_asset) {
      try {
         // Initialize for the current time
         trx.clear();
         set_expiration(db, trx);

         // Initialize actors
         ACTORS((rsquaredchp1)(feedproducer));

         // Initialize tokens
         create_user_issued_asset( "SMARTBIT", rsquaredchp1, 0 );
         // Obtain asset object after a block is generated to obtain the final object that is commited to the database
         generate_block();
         const asset_object &bitsmart = get_asset("SMARTBIT");

         generate_block();


         //////
         // Before HF, test inability to set taker fees
         //////
         asset_update_operation uop;
         uop.issuer = rsquaredchp1.id;
         uop.asset_to_update = bitsmart.get_id();
         uop.new_options = bitsmart.options;
         uint16_t new_taker_fee_percent = uop.new_options.market_fee_percent / 2;
         uop.new_options.extensions.value.taker_fee_percent = new_taker_fee_percent;

         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, rsquaredchp1_private_key);
         PUSH_TX(db, trx);

         // Check the taker fee
         asset_object updated_asset = bitsmart.get_id()(db);
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());


         generate_block();
         trx.clear();
         set_expiration(db, trx);


         //////
         // After HF, test default values of taker fee after HF
         // After the HF its default value should still not be set
         //////
         updated_asset = bitsmart.get_id()(db);
         uint16_t expected_taker_fee_percent = updated_asset.options.market_fee_percent;
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());


         //////
         // After HF, test invalid taker fees
         //////
         uop.new_options.extensions.value.taker_fee_percent = GRAPHENE_100_PERCENT + 1;
         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, rsquaredchp1_private_key);
         GRAPHENE_CHECK_THROW(PUSH_TX(db, trx), fc::exception); // An exception should be thrown indicating the reason
         // TODO: Check the specific exception?


         //////
         // After HF, test that new values can be set
         //////
         new_taker_fee_percent = uop.new_options.market_fee_percent / 4;
         uop.new_options.extensions.value.taker_fee_percent = new_taker_fee_percent;
         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, rsquaredchp1_private_key);
         PUSH_TX(db, trx); // No exception should be thrown

         // Check the taker fee
         updated_asset = bitsmart.get_id()(db);
         expected_taker_fee_percent = new_taker_fee_percent;
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());
         BOOST_CHECK_EQUAL(expected_taker_fee_percent, *updated_asset.options.extensions.value.taker_fee_percent);

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Test the default taker fee values of multiple different assets after HF
    */
   BOOST_AUTO_TEST_CASE(default_taker_fees) {
      try {
         // Initialize for the current time
         trx.clear();
         set_expiration(db, trx);

         // Initialize actors
         ACTORS((rsquaredchp1)(bob)(charlie)(smartissuer));

         // Initialize tokens with custom market fees
         price price(asset(1, asset_id_type(1)), asset(1));

         const uint16_t rsquaredchp11coin_market_fee_percent = 1 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("RSQRCHP11COIN", rsquaredchp1, charge_market_fee, price, 2,
                                                                  rsquaredchp11coin_market_fee_percent);

         const uint16_t rsquaredchp12coin_market_fee_percent = 2 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("RSQRCHP12COIN", rsquaredchp1, charge_market_fee, price, 2,
                                                                  rsquaredchp12coin_market_fee_percent);

         const uint16_t bob1coin_market_fee_percent = 3 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("BOB1COIN", rsquaredchp1, charge_market_fee, price, 2,
                                                                bob1coin_market_fee_percent);

         const uint16_t bob2coin_market_fee_percent = 4 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("BOB2COIN", rsquaredchp1, charge_market_fee, price, 2,
                                                                bob2coin_market_fee_percent);

         const uint16_t charlie1coin_market_fee_percent = 4 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("CHARLIE1COIN", rsquaredchp1, charge_market_fee, price, 2,
                                                                    charlie1coin_market_fee_percent);

         const uint16_t charlie2coin_market_fee_percent = 5 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("CHARLIE2COIN", rsquaredchp1, charge_market_fee, price, 2,
                                                                    charlie2coin_market_fee_percent);
         // Obtain asset object after a block is generated to obtain the final object that is commited to the database
         generate_block();
         const asset_object& rsquaredchp11coin = get_asset("RSQRCHP11COIN");
         const asset_object& rsquaredchp12coin = get_asset("RSQRCHP12COIN");
         const asset_object& bob1coin = get_asset("BOB1COIN");
         const asset_object& bob2coin = get_asset("BOB2COIN");
         const asset_object& charlie1coin = get_asset("CHARLIE1COIN");
         const asset_object& charlie2coin = get_asset("CHARLIE2COIN");
         //////
         // Before HF, test the market/maker fees for each asset
         //////
         asset_object updated_asset;
         uint16_t expected_fee_percent;

         updated_asset = rsquaredchp11coin.get_id()(db);
         expected_fee_percent = rsquaredchp11coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);

         updated_asset = rsquaredchp12coin.get_id()(db);
         expected_fee_percent = rsquaredchp12coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);

         updated_asset = bob1coin.get_id()(db);
         expected_fee_percent = bob1coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);

         updated_asset = bob2coin.get_id()(db);
         expected_fee_percent = bob2coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);

         updated_asset = charlie1coin.get_id()(db);
         expected_fee_percent = charlie1coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);

         updated_asset = charlie2coin.get_id()(db);
         expected_fee_percent = charlie2coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);

         //////
         // Before HF, test that taker fees are not set
         //////
         // Check the taker fee
         updated_asset = rsquaredchp11coin.get_id()(db);
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         updated_asset = rsquaredchp12coin.get_id()(db);
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         updated_asset = bob1coin.get_id()(db);
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         updated_asset = bob2coin.get_id()(db);
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         updated_asset = charlie1coin.get_id()(db);
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         updated_asset = charlie2coin.get_id()(db);
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         //////
         // Advance to activate hardfork
         //////
         generate_block();
         trx.clear();
         set_expiration(db, trx);


         //////
         // After HF, test the maker fees for each asset are unchanged
         //////
         updated_asset = rsquaredchp11coin.get_id()(db);
         expected_fee_percent = rsquaredchp11coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);

         updated_asset = rsquaredchp12coin.get_id()(db);
         expected_fee_percent = rsquaredchp12coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);

         updated_asset = bob1coin.get_id()(db);
         expected_fee_percent = bob1coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);

         updated_asset = bob2coin.get_id()(db);
         expected_fee_percent = bob2coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);

         updated_asset = charlie1coin.get_id()(db);
         expected_fee_percent = charlie1coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);

         updated_asset = charlie2coin.get_id()(db);
         expected_fee_percent = charlie2coin_market_fee_percent;
         BOOST_CHECK_EQUAL(expected_fee_percent, updated_asset.options.market_fee_percent);

         //////
         // After HF, test the taker fees for each asset are not set
         //////
         updated_asset = rsquaredchp11coin.get_id()(db);
         expected_fee_percent = rsquaredchp11coin_market_fee_percent;
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         updated_asset = rsquaredchp12coin.get_id()(db);
         expected_fee_percent = rsquaredchp12coin_market_fee_percent;
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         updated_asset = bob1coin.get_id()(db);
         expected_fee_percent = bob1coin_market_fee_percent;
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         updated_asset = bob2coin.get_id()(db);
         expected_fee_percent = bob2coin_market_fee_percent;
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         updated_asset = charlie1coin.get_id()(db);
         expected_fee_percent = charlie1coin_market_fee_percent;
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         updated_asset = charlie2coin.get_id()(db);
         expected_fee_percent = charlie2coin_market_fee_percent;
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Test of different maker and taker fees charged when filling limit orders after HF for a UIA
    */
   BOOST_AUTO_TEST_CASE(simple_match_and_fill_with_different_fees_uia_1) {
      try {
         // Initialize for the current time
         trx.clear();
         set_expiration(db, trx);

         // Initialize actors
         ACTORS((jill)(izzy)(alice)(bob)(rsquaredchp1));

         // Initialize tokens
         price price(asset(1, asset_id_type(1)), asset(1));

         // UNUSED: const uint16_t JILL_PRECISION = 100;
         const uint16_t jill_market_fee_percent = 2 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("JCOIN", rsquaredchp1, charge_market_fee, price, 2,
                                                                jill_market_fee_percent);

         // UNUSED: const uint16_t IZZY_PRECISION = 1000;
         const uint16_t izzy_market_fee_percent = 5 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("ICOIN", rsquaredchp1, charge_market_fee, price, 3,
                                                                izzy_market_fee_percent);

         // Obtain asset object after a block is generated to obtain the final object that is commited to the database
         generate_block();
         const asset_object& jillcoin = get_asset("JCOIN");
         const asset_object& izzycoin = get_asset("ICOIN");


         //////
         // Advance to activate hardfork
         //////
         generate_block();
         trx.clear();
         set_expiration(db, trx);


         //////
         // After HF, test that new values can be set
         //////
         // Define the new taker fees
         uint16_t jill_maker_fee_percent = jill_market_fee_percent;
         uint16_t jill_taker_fee_percent = jill_maker_fee_percent / 2;

         uint16_t izzy_maker_fee_percent = izzy_market_fee_percent;
         uint16_t izzy_taker_fee_percent = izzy_maker_fee_percent / 2;

         // Set the new taker fee for JILLCOIN
         asset_update_operation uop;
         uop.issuer = rsquaredchp1.id;
         uop.asset_to_update = jillcoin.get_id();
         uop.new_options = jillcoin.options;
         uop.new_options.extensions.value.taker_fee_percent = jill_taker_fee_percent;

         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, rsquaredchp1_private_key);
         PUSH_TX(db, trx); // No exception should be thrown

         // Check the taker fee for JILLCOIN
         asset_object updated_asset = jillcoin.get_id()(db);
         uint16_t expected_taker_fee_percent = jill_taker_fee_percent;
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());
         BOOST_CHECK_EQUAL(expected_taker_fee_percent, *updated_asset.options.extensions.value.taker_fee_percent);

         // Set the new taker fee for IZZYCOIN
         uop.issuer = rsquaredchp1.id;
         uop.asset_to_update = izzycoin.get_id();
         uop.new_options = izzycoin.options;
         uop.new_options.extensions.value.taker_fee_percent = izzy_taker_fee_percent;

         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, rsquaredchp1_private_key);
         PUSH_TX(db, trx); // No exception should be thrown

         // Check the taker fee for IZZYCOIN
         updated_asset = izzycoin.get_id()(db);
         expected_taker_fee_percent = izzy_taker_fee_percent;
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());
         BOOST_CHECK_EQUAL(expected_taker_fee_percent, *updated_asset.options.extensions.value.taker_fee_percent);

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Test of different maker and taker fees charged when filling limit orders after HF for a UIA
    *
    * Test the filling of a taker fee when the **maker** fee percent is set to 0.  This tests some optimizations
    * in database::calculate_market_fee().
    */
   BOOST_AUTO_TEST_CASE(simple_match_and_fill_with_different_fees_uia_2) {
      try {
         // Initialize for the current time
         trx.clear();
         set_expiration(db, trx);

         // Initialize actors
         ACTORS((jill)(izzy)(alice)(bob)(rsquaredchp1));

         // Initialize tokens
         price price(asset(1, asset_id_type(1)), asset(1));

         // UNUSED: const uint16_t JILL_PRECISION = 100;
         const uint16_t jill_market_fee_percent = 0 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("JCOIN", rsquaredchp1, charge_market_fee, price, 2,
                                                                jill_market_fee_percent);

         // UNUSED: const uint16_t IZZY_PRECISION = 1000;
         const uint16_t izzy_market_fee_percent = 0 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("ICOIN", rsquaredchp1, charge_market_fee, price, 3,
                                                                izzy_market_fee_percent);

         // Obtain asset object after a block is generated to obtain the final object that is commited to the database
         generate_block();
         const asset_object& jillcoin = get_asset("JCOIN");
         const asset_object& izzycoin = get_asset("ICOIN");


         //////
         // Advance to activate hardfork
         //////
         generate_block();
         trx.clear();
         set_expiration(db, trx);


         //////
         // After HF, test that new values can be set
         //////
         // Define the new taker fees
         uint16_t jill_maker_fee_percent = jill_market_fee_percent;
         uint16_t jill_taker_fee_percent = 1 * GRAPHENE_1_PERCENT;

         uint16_t izzy_maker_fee_percent = izzy_market_fee_percent;
         uint16_t izzy_taker_fee_percent = 3 * GRAPHENE_1_PERCENT;

         // Set the new taker fee for JILLCOIN
         asset_update_operation uop;
         uop.issuer = rsquaredchp1.id;
         uop.asset_to_update = jillcoin.get_id();
         uop.new_options.market_fee_percent = jill_maker_fee_percent;
         uop.new_options = jillcoin.options;
         uop.new_options.extensions.value.taker_fee_percent = jill_taker_fee_percent;

         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, rsquaredchp1_private_key);
         PUSH_TX(db, trx); // No exception should be thrown

         // Check the taker fee for JILLCOIN
         asset_object updated_asset = jillcoin.get_id()(db);
         uint16_t expected_taker_fee_percent = jill_taker_fee_percent;
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());
         BOOST_CHECK_EQUAL(expected_taker_fee_percent, *updated_asset.options.extensions.value.taker_fee_percent);

         // Set the new taker fee for IZZYCOIN
         uop.issuer = rsquaredchp1.id;
         uop.asset_to_update = izzycoin.get_id();
         uop.new_options.market_fee_percent = izzy_maker_fee_percent;
         uop.new_options = izzycoin.options;
         uop.new_options.extensions.value.taker_fee_percent = izzy_taker_fee_percent;

         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, rsquaredchp1_private_key);
         PUSH_TX(db, trx); // No exception should be thrown

         // Check the taker fee for IZZYCOIN
         updated_asset = izzycoin.get_id()(db);
         expected_taker_fee_percent = izzy_taker_fee_percent;
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());
         BOOST_CHECK_EQUAL(expected_taker_fee_percent, *updated_asset.options.extensions.value.taker_fee_percent);

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Test of different maker and taker fees charged when filling limit orders after HF for a UIA
    *
    * Test the filling of a taker fee when the **taker** fee percent is set to 0.  This tests some optimizations
    * in database::calculate_market_fee().
    */
   BOOST_AUTO_TEST_CASE(simple_match_and_fill_with_different_fees_uia_3) {
      try {
         // Initialize for the current time
         trx.clear();
         set_expiration(db, trx);

         // Initialize actors
         ACTORS((jill)(izzy)(alice)(bob)(rsquaredchp1));

         // Initialize tokens
         price price(asset(1, asset_id_type(1)), asset(1));

         // UNUSED: const uint16_t JILL_PRECISION = 100;
         const uint16_t jill_market_fee_percent = 2 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("JCOIN", rsquaredchp1, charge_market_fee, price, 2,
                                                                jill_market_fee_percent);

         // UNUSED: const uint16_t IZZY_PRECISION = 1000;
         const uint16_t izzy_market_fee_percent = 5 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("ICOIN", rsquaredchp1, charge_market_fee, price, 3,
                                                                izzy_market_fee_percent);

         // Obtain asset object after a block is generated to obtain the final object that is commited to the database
         generate_block();
         const asset_object& jillcoin = get_asset("JCOIN");
         const asset_object& izzycoin = get_asset("ICOIN");


         //////
         // Advance to activate hardfork
         //////
         generate_block();
         trx.clear();
         set_expiration(db, trx);


         //////
         // After HF, test that new values can be set
         //////
         // Define the new taker fees
         uint16_t jill_maker_fee_percent = jill_market_fee_percent;
         uint16_t jill_taker_fee_percent = 0 * GRAPHENE_1_PERCENT;

         uint16_t izzy_maker_fee_percent = izzy_market_fee_percent;
         uint16_t izzy_taker_fee_percent = 0 * GRAPHENE_1_PERCENT;

         // Set the new taker fee for JILLCOIN
         asset_update_operation uop;
         uop.issuer = rsquaredchp1.id;
         uop.asset_to_update = jillcoin.get_id();
         uop.new_options.market_fee_percent = jill_maker_fee_percent;
         uop.new_options = jillcoin.options;
         uop.new_options.extensions.value.taker_fee_percent = jill_taker_fee_percent;

         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, rsquaredchp1_private_key);
         PUSH_TX(db, trx); // No exception should be thrown

         // Check the taker fee for JILLCOIN
         asset_object updated_asset = jillcoin.get_id()(db);
         uint16_t expected_taker_fee_percent = jill_taker_fee_percent;
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());
         BOOST_CHECK_EQUAL(expected_taker_fee_percent, *updated_asset.options.extensions.value.taker_fee_percent);

         // Set the new taker fee for IZZYCOIN
         uop.issuer = rsquaredchp1.id;
         uop.asset_to_update = izzycoin.get_id();
         uop.new_options.market_fee_percent = izzy_maker_fee_percent;
         uop.new_options = izzycoin.options;
         uop.new_options.extensions.value.taker_fee_percent = izzy_taker_fee_percent;

         trx.clear();
         trx.operations.push_back(uop);
         db.current_fee_schedule().set_fee(trx.operations.back());
         sign(trx, rsquaredchp1_private_key);
         PUSH_TX(db, trx); // No exception should be thrown

         // Check the taker fee for IZZYCOIN
         updated_asset = izzycoin.get_id()(db);
         expected_taker_fee_percent = izzy_taker_fee_percent;
         BOOST_CHECK(updated_asset.options.extensions.value.taker_fee_percent.valid());
         BOOST_CHECK_EQUAL(expected_taker_fee_percent, *updated_asset.options.extensions.value.taker_fee_percent);

      } FC_LOG_AND_RETHROW()
   }


   /**
    * Test of **default** taker fees charged when filling limit orders after HF for a UIA.
    *
    * This test is similar to simple_match_and_fill_with_different_fees_uia_1
    * except that the taker fee is not explicitly set and instead defaults to the maker fee.
    */
   BOOST_AUTO_TEST_CASE(simple_match_and_fill_with_different_fees_uia_4) {
      try {
         // Initialize for the current time
         trx.clear();
         set_expiration(db, trx);

         // Initialize actors
         ACTORS((jill)(izzy)(alice)(bob)(rsquaredchp1));

         // Initialize tokens
         price price(asset(1, asset_id_type(1)), asset(1));

         const uint16_t JILL_PRECISION = 100;
         const uint16_t jill_market_fee_percent = 2 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("JCOIN", rsquaredchp1, charge_market_fee, price, 2,
                                  jill_market_fee_percent);

         const uint16_t IZZY_PRECISION = 1000;
         const uint16_t izzy_market_fee_percent = 5 * GRAPHENE_1_PERCENT;
         create_user_issued_asset("ICOIN", rsquaredchp1, charge_market_fee, price, 3,
                                  izzy_market_fee_percent);

         // Obtain asset object after a block is generated to obtain the final object that is commited to the database
         generate_block();
         const asset_object& jillcoin = get_asset("JCOIN");
         const asset_object& izzycoin = get_asset("ICOIN");


         //////
         // Advance to activate hardfork
         //////
         generate_block();
         trx.clear();
         set_expiration(db, trx);


         //////
         // After HF, test that default taker values has not been set
         //////
         // The taker fees should automatically default to maker fees if the taker fee is not explicitly set
         // UNUSED: uint16_t jill_maker_fee_percent = jill_market_fee_percent;
         // UNUSED: uint16_t jill_taker_fee_percent = jill_market_fee_percent;

         // UNUSED: uint16_t izzy_maker_fee_percent = izzy_market_fee_percent;
         // UNUSED: uint16_t izzy_taker_fee_percent = izzy_market_fee_percent;

         // Check the taker fee for JCOIN: it should still not be set
         asset_object updated_asset = jillcoin.get_id()(db);
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());

         // Check the taker fee for ICOIN: it should still not be set
         updated_asset = izzycoin.get_id()(db);
         BOOST_CHECK(!updated_asset.options.extensions.value.taker_fee_percent.valid());


         //////
         // After HF, create limit orders that will perfectly match
         //////
         BOOST_TEST_MESSAGE("Issuing 10 jillcoin to alice");
         issue_uia(alice, jillcoin.amount(10 * JILL_PRECISION));
         BOOST_TEST_MESSAGE("Checking alice's balance");
         BOOST_REQUIRE_EQUAL(get_balance(alice, jillcoin), 10 * JILL_PRECISION);

         BOOST_TEST_MESSAGE("Issuing 300 izzycoin to bob");
         issue_uia(bob, izzycoin.amount(300 * IZZY_PRECISION));
         BOOST_TEST_MESSAGE("Checking bob's balance");
         BOOST_REQUIRE_EQUAL(get_balance(bob, izzycoin), 300 * IZZY_PRECISION);

      } FC_LOG_AND_RETHROW()
   }

BOOST_AUTO_TEST_SUITE_END()
