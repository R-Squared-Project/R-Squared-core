/*
 * Copyright (c) 2018-2023 Revolution Populi Limited, and contributors.
 * Copyright (c) 2023 R-Squared Labs LLC <rsquaredlabscontact@gmail.com>, and contributors.
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

#pragma once

#include <graphene/chain/types.hpp>
#include <graphene/db/generic_index.hpp>
#include <graphene/protocol/account.hpp>

#include <boost/multi_index/composite_key.hpp>

namespace graphene { namespace chain {
        class database;
        class personal_data_object;
        class vesting_balance_object;

        /**
         * @brief This class represents an pensonal data on the object graph
         * @ingroup object
         * @ingroup protocol
         *
         * Personal data the primary unit to give and store permissions to account's personal data.
         */
        class personal_data_object : public graphene::db::abstract_object<personal_data_object>
        {
        public:
            static const uint8_t space_id = protocol_ids;
            static const uint8_t type_id  = personal_data_object_type;

            account_id_type subject_account;
            account_id_type operator_account;
            string url;
            string hash;
            string storage_data;
        };

        struct by_subject_account;
        struct by_operator_account;

        typedef multi_index_container<
               personal_data_object,
               indexed_by<
                     ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
                     ordered_unique< tag<by_subject_account>,
                           composite_key< personal_data_object,
                                 member< personal_data_object, account_id_type, &personal_data_object::subject_account>,
                                 member< personal_data_object, account_id_type, &personal_data_object::operator_account>,
                                 member< personal_data_object, string, &personal_data_object::hash>
                           >
                     >,
                     ordered_unique< tag<by_operator_account>,
                           composite_key< personal_data_object,
                                 member< personal_data_object, account_id_type, &personal_data_object::operator_account>,
                                 member< personal_data_object, account_id_type, &personal_data_object::subject_account>,
                                 member< personal_data_object, string, &personal_data_object::hash>
                           >
                     >
               >
        > personal_data_multi_index_type;

        typedef generic_index<personal_data_object, personal_data_multi_index_type> personal_data_index;

    }}

MAP_OBJECT_ID_TO_TYPE(graphene::chain::personal_data_object)
FC_REFLECT_TYPENAME( graphene::chain::personal_data_object )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::personal_data_object )
