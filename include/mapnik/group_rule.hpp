/*****************************************************************************
 *
 * This file is part of Mapnik (c++ mapping toolkit)
 *
 * Copyright (C) 2012 Artem Pavlenko
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *****************************************************************************/

#ifndef MAPNIK_GROUP_RULE_HPP
#define MAPNIK_GROUP_RULE_HPP

// mapnik
#include <mapnik/symbolizer.hpp>
#include <mapnik/expression.hpp>
#include <mapnik/symbolizer_variant.hpp>

// boost
#include <boost/shared_ptr.hpp>

namespace mapnik
{

/**
 * group rule contains a set of symbolizers which should
 * be rendered atomically when the filter attached to
 * this rule is matched.
 */
struct group_rule
{
   typedef std::vector<mapnik::symbolizer> symbolizers;

   explicit group_rule(const expression_ptr& filter);

   group_rule &operator=(const group_rule &rhs);
   bool operator==(const group_rule &rhs) const;

   void append(const symbolizer &);

   const symbolizers &get_symbolizers() const
   {
      return symbolizers_;
   }

   inline symbolizers::const_iterator begin() const 
   {
      return symbolizers_.begin();
   }

   inline symbolizers::const_iterator end() const
   {
      return symbolizers_.end();
   }

   inline void set_filter(const expression_ptr& filter)
   {
      filter_ = filter;
   }
   
   inline expression_ptr const& get_filter() const
   {
      return filter_;
   }
   
private:

   // expression filter - when data matches this then
   // the symbolizers should be drawn.
   expression_ptr filter_;

   // the atomic set of symbolizers
   symbolizers symbolizers_;
};
}

#endif // MAPNIK_GROUP_RULE_HPP
