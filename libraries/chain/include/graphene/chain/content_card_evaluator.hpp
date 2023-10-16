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
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/content_card_object.hpp>

namespace graphene { namespace chain {

class content_card_create_evaluator : public evaluator<content_card_create_evaluator>
{
public:
   typedef content_card_create_operation operation_type;

   void_result do_evaluate( const content_card_create_operation& o );
   object_id_type do_apply( const content_card_create_operation& o );
};

class content_card_update_evaluator : public evaluator<content_card_update_evaluator>
{
public:
   typedef content_card_update_operation operation_type;

   void_result do_evaluate( const content_card_update_operation& o );
   object_id_type do_apply( const content_card_update_operation& o );
};

class content_card_remove_evaluator : public evaluator<content_card_remove_evaluator>
{
public:
   typedef content_card_remove_operation operation_type;

   void_result do_evaluate( const content_card_remove_operation& o );
   object_id_type do_apply( const content_card_remove_operation& o );
};

} } // graphene::chain
