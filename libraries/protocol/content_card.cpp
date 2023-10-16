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

#include <graphene/protocol/content_card.hpp>

#include <fc/io/raw.hpp>

namespace graphene { namespace protocol {

share_type content_card_create_operation::calculate_fee( const fee_parameters_type& k )const
{
   return k.fee + calculate_data_fee( fc::raw::pack_size(url), k.price_per_kbyte );
}

void content_card_create_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
}


share_type content_card_update_operation::calculate_fee( const fee_parameters_type& k )const
{
   return k.fee + calculate_data_fee( fc::raw::pack_size(url), k.price_per_kbyte );
}

void content_card_update_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
}


share_type content_card_remove_operation::calculate_fee( const fee_parameters_type& k )const
{
   return 0;
}

void content_card_remove_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
}

} } // graphene::protocol

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::content_card_create_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::content_card_create_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::content_card_update_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::content_card_update_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::content_card_remove_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::content_card_remove_operation )
