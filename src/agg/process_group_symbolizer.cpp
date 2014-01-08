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
#include <mapnik/group/group_layout_manager.hpp>
#include <mapnik/text/layout.hpp>

#include <mapnik/geom_util.hpp>
#include <mapnik/symbolizer.hpp>
#include <mapnik/parse_path.hpp>
#include <mapnik/pixel_position.hpp>
#include <mapnik/label_collision_detector.hpp>

#include <mapnik/renderer_common/process_point_symbolizer.hpp>
#include <mapnik/text/symbolizer_helpers.hpp>

// agg
#include "agg_trans_affine.h"

// stl
#include <string>

// boost
#include <boost/variant/apply_visitor.hpp>

namespace mapnik {

/* General:
 *
 * The approach here is to run the normal symbolizers, but in
 * a 'virtual' blank environment where the changes that they
 * make are recorded (the detector, the render_* calls).
 *
 * The recorded boxes are then used to lay out the items and
 * the offsets from old to new positions can be used to perform
 * the actual rendering calls.
 *
 * This should allow us to re-use as much as possible of the
 * existing symbolizer layout and rendering code while still
 * being able to interpose our own decisions about whether
 * a collision has occured or not.
 */

/**
 * Thunk for rendering a particular instance of a point - this
 * stores all the arguments necessary to re-render this point
 * symbolizer at a later time.
 */
struct point_render_thunk
{
    pixel_position pos_;
    marker_ptr marker_;
    agg::trans_affine tr_;
    double opacity_;
    composite_mode_e comp_op_;

    point_render_thunk(pixel_position const &pos, marker const &m,
                       agg::trans_affine const &tr, double opacity,
                       composite_mode_e comp_op)
        : pos_(pos), marker_(std::make_shared<marker>(m)),
          tr_(tr), opacity_(opacity), comp_op_(comp_op)
    {}
};

struct text_render_thunk
{
    placements_list placements_;
    halo_rasterizer_enum halo_rasterizer_;
    composite_mode_e comp_op_;
    
    text_render_thunk(placements_list const &placements,
                      halo_rasterizer_enum halo_rasterizer,
                      composite_mode_e comp_op)
        : placements_(placements), halo_rasterizer_(halo_rasterizer),
          comp_op_(comp_op)
    {}
};

// Variant type for render thunks to allow us to re-render them
// via a static visitor later.
typedef boost::variant<point_render_thunk,
                       text_render_thunk> render_thunk;
typedef std::shared_ptr<render_thunk> render_thunk_ptr;
typedef std::list<render_thunk_ptr> render_thunk_list;

/**
 * Visitor to extract the bounding boxes associated with placing
 * a symbolizer at a fake, virtual point - not real geometry.
 *
 * The bounding boxes can be used for layout, and the thunks are
 * used to re-render at locations according to the group layout.
 */
struct extract_bboxes : public boost::static_visitor<>
{
    extract_bboxes(box2d<double> &box,
                   render_thunk_list &thunks,
                   mapnik::feature_impl &feature,
                   proj_transform const &prj_trans,
                   renderer_common const &common,
                   text_layout const &text,
                   box2d<double> const &clipping_extent)
        : box_(box), thunks_(thunks), feature_(feature), prj_trans_(prj_trans),
          common_(common), text_(text), clipping_extent_(clipping_extent)
    {}

    void operator()(point_symbolizer const &sym) const
    {
        // create an empty detector, so we are sure we won't hit
        // anything
        renderer_common common(common_);
        common.detector_->clear();

        composite_mode_e comp_op = get<composite_mode_e>(sym, keys::comp_op, feature_, src_over);

        render_point_symbolizer(
            sym, feature_, prj_trans_, common,
            [&](pixel_position const &pos, marker const &marker,
                agg::trans_affine const &tr, double opacity) {
                point_render_thunk thunk(pos, marker, tr, opacity, comp_op);
                thunks_.push_back(std::make_shared<render_thunk>(std::move(thunk)));
            });

        update_box(*common.detector_);
    }

    void operator()(text_symbolizer const &sym) const
    {
        // create an empty detector, so we are sure we won't hit
        // anything
        renderer_common common(common_);
        common.detector_->clear();

        box2d<double> clip_box = clipping_extent_;
        text_symbolizer_helper helper(
            sym, feature_, prj_trans_,
            common.width_, common.height_,
            common.scale_factor_,
            common.t_, common.font_manager_, *common.detector_,
            clip_box);

        halo_rasterizer_enum halo_rasterizer = get<halo_rasterizer_enum>(sym, keys::halo_rasterizer, HALO_RASTERIZER_FULL);
        composite_mode_e comp_op = get<composite_mode_e>(sym, keys::comp_op, feature_, src_over);
        
        placements_list const& placements = helper.get();
        text_render_thunk thunk(placements, halo_rasterizer, comp_op);
        thunks_.push_back(std::make_shared<render_thunk>(std::move(thunk)));

        update_box(*common.detector_);
    }

    template <typename T>
    void operator()(T const &sym) const
    {
        // TODO: warning if unimplemented?
    }

private:
    box2d<double> &box_;
    render_thunk_list &thunks_;
    mapnik::feature_impl &feature_;
    proj_transform const &prj_trans_;
    renderer_common const &common_;
    text_layout const &text_;
    box2d<double> clipping_extent_;

    void update_box(label_collision_detector4 &detector) const
    {
        for (auto const &label : detector)
        {
            box_.expand_to_include(label.box);
        }
    }
};

geometry_type *origin_point(proj_transform const &prj_trans, 
                            renderer_common const &common)
{
    // note that we choose a point in the middle of the screen to
    // try to ensure that we don't get edge artefacts due to any
    // symbolizers with avoid-edges set: only the avoid-edges of
    // the group symbolizer itself should matter.
    double x = common.width_ / 2.0, y = common.height_ / 2.0, z = 0.0;
    common.t_.backward(&x, &y);
    prj_trans.forward(x, y, z);
    geometry_type *geom = new geometry_type(geometry_type::Point);
    geom->move_to(x, y);
    return geom;
}

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

    // keep track of the sub features that we'll want to symbolize
    // along with the group rules that they matched
    std::vector< std::pair<group_rule_ptr, feature_ptr> > matches;

    // layout manager to store and arrange bboxes of matched features
    group_layout_manager layout_manager(props->get_layout());
    text_layout text(common_.font_manager_, common_.scale_factor_);

    // run feature or sub feature through the group rules & symbolizers
    // for each index value in the range
    int start = get<value_integer>(sym, keys::start_column);
    int end = start + get<value_integer>(sym, keys::num_columns);
    for (int col_idx = start; col_idx < end; ++col_idx)
    {
        feature_ptr sub_feature;

        if (has_idx_cols)
        {
            // create sub feature with indexed column values
            sub_feature = feature_factory::create(sub_feature_ctx, col_idx);

            // copy the necessary columns to sub feature
            for(auto const& col_name : columns)
            {
                if (col_name.find('%') != std::string::npos)
                {
                    if (col_name.size() == 1)
                    {
                        // column name is '%' by itself, so give the index as the value
                        sub_feature->put(col_name, (value_integer)col_idx);
                    }
                    else
                    {
                        // indexed column
                        std::string col_idx_name = col_name;
                        boost::replace_all(col_idx_name, "%", boost::lexical_cast<std::string>(col_idx));
                        sub_feature->put(col_name, feature.get(col_idx_name));
                    }
                }
                else
                {
                    // non-indexed column
                    sub_feature->put(col_name, feature.get(col_name));
                }
            }
        }
        else
        {
            // no indexed columns, so use the existing feature instead of copying
            sub_feature = feature_ptr(&feature);
        }

        // add a single point geometry at pixel origin
        sub_feature->add_geometry(origin_point(prj_trans, common_));

        // get the layout for this set of properties
        group_layout const &layout = props->get_layout();

        for (auto const& rule : props->get_rules())
        {
             if (boost::apply_visitor(evaluate<Feature,value_type>(*sub_feature),
                                               *(rule->get_filter())).to_bool())
             {
                // add matched rule and feature to the list of things to draw
                matches.push_back(std::make_pair(rule, sub_feature));

                // construct a bounding box around all symbolizers for the matched rule
                bound_box bounds;
                render_thunk_list thunks;
                extract_bboxes extractor(bounds, thunks, *sub_feature, prj_trans,
                                         common_, text, clipping_extent());

                for (auto const& sym : *rule)
                {
                    // TODO: construct layout and obtain bounding box
                    boost::apply_visitor(extractor, sym);
                }

                // add the bounding box to the layout manager
                layout_manager.add_member_bound_box(bounds);
                break;
            }
        }
    }
}

template void agg_renderer<image_32>::process(group_symbolizer const&,
                                              mapnik::feature_impl &,
                                              proj_transform const&);

}
