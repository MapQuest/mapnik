/*****************************************************************************
 *
 * This file is part of Mapnik (c++ mapping toolkit)
 *
 * Copyright (C) 2011 Artem Pavlenko
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

// mapnik
#include <mapnik/feature.hpp>
#include <mapnik/agg_renderer.hpp>
#include <mapnik/agg_rasterizer.hpp>
#include <mapnik/image_util.hpp>
#include <mapnik/feature_factory.hpp>
#include <mapnik/attribute_collector.hpp>

#include <mapnik/geom_util.hpp>
#include <mapnik/symbolizer.hpp>
#include <mapnik/parse_path.hpp>
#include <mapnik/pixel_position.hpp>

// agg
#include "agg_trans_affine.h"

// stl
#include <string>

// boost
#include <boost/variant/apply_visitor.hpp>

namespace mapnik {

template <typename T0, typename T1>
void agg_renderer<T0,T1>::process(group_symbolizer const& sym,
                                  mapnik::feature_impl & feature,
                                  proj_transform const& prj_trans)
{
    // find all column names referenced in the group rules and symbolizers
    std::set<std::string> columns;
    attribute_collector column_collector(columns);
    expression_attributes<std::set<std::string> > rk_attr(columns);

    expression_ptr repeat_key = get<mapnik::expression_ptr>(sym, keys::repeat_key);
    if (repeat_key)
    {
        boost::apply_visitor(rk_attr, *repeat_key);
    }

    // get columns from child rules and symbolizers
    group_symbolizer_properties_ptr props = get<group_symbolizer_properties_ptr>(sym, keys::group_properties);
    if (props) {
        for (auto const& rule : props->get_rules())
        {
            // note that this recurses down on to the symbolizer
            // internals too, so we get all free variables.
            column_collector(*rule);
            // still need to collect repeat key columns
            if (rule->get_repeat_key())
            {
                boost::apply_visitor(rk_attr, *(rule->get_repeat_key()));
            }
        }
    }

    // create a new context for the sub features of this group
    context_ptr sub_feature_ctx = std::make_shared<mapnik::context_type>();

    // populate new context with column names referenced in the group rules and symbolizers
    // and determine if any indexed columns are present
    bool has_idx_cols = false;
    for (auto const& col_name : columns)
    {
        sub_feature_ctx->push(col_name);

        if (col_name.find('%') != std::string::npos)
        {
            has_idx_cols = true;
        }
    }
}

template void agg_renderer<image_32>::process(group_symbolizer const&,
                                              mapnik::feature_impl &,
                                              proj_transform const&);

}
