/*****************************************************************************
 *
 * This file is part of Mapnik (c++ mapping toolkit)
 *
 * Copyright (C) 2013 Artem Pavlenko
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

#ifndef MAPNIK_RENDERER_COMMON_PROCESS_GROUP_SYMBOLIZER_HPP
#define MAPNIK_RENDERER_COMMON_PROCESS_GROUP_SYMBOLIZER_HPP

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
struct common_point_render_thunk
{
    pixel_position pos_;
    marker_ptr marker_;
    agg::trans_affine tr_;
    double opacity_;

    common_point_render_thunk(pixel_position const &pos, marker const &m,
                              agg::trans_affine const &tr, double opacity)
        : pos_(pos), marker_(std::make_shared<marker>(m)),
          tr_(tr), opacity_(opacity)
    {}
};

struct common_text_render_thunk
{
    // need to keep these around, annoyingly, as the glyph_position
    // struct keeps a pointer to the glyph_info, so we have to
    // ensure the lifetime is the same.
    placements_list placements_;
    std::shared_ptr<std::vector<glyph_info> > glyphs_;

    common_text_render_thunk(placements_list const &placements)
        : placements_(), glyphs_(std::make_shared<std::vector<glyph_info> >())
    {
        std::vector<glyph_info> &glyph_vec = *glyphs_;

        size_t glyph_count = 0;
        for (glyph_positions_ptr positions : placements)
        {
            glyph_count += std::distance(positions->begin(), positions->end());
        }
        glyph_vec.reserve(glyph_count);

        for (glyph_positions_ptr positions : placements)
        {
            glyph_positions_ptr new_positions = std::make_shared<glyph_positions>();
            new_positions->reserve(std::distance(positions->begin(), positions->end()));
            glyph_positions &new_pos = *new_positions;

            new_pos.set_base_point(positions->get_base_point());
            if (positions->marker())
            {
                new_pos.set_marker(positions->marker(), positions->marker_pos());
            }

            for (glyph_position const &pos : *positions)
            {
                glyph_vec.push_back(*pos.glyph);
                new_pos.push_back(glyph_vec.back(), pos.pos, pos.rot);
            }

            placements_.push_back(new_positions);
        }
    }
};

/**
 * Base class for extracting the bounding boxes associated with placing
 * a symbolizer at a fake, virtual point - not real geometry.
 *
 * The bounding boxes can be used for layout, and the thunks are
 * used to re-render at locations according to the group layout.
 */
template <typename T>
struct common_render_thunk_extractor
{
	typedef T render_thunk;
	typedef std::shared_ptr<render_thunk> render_thunk_ptr;
	typedef std::list<render_thunk_ptr> render_thunk_list;

	common_render_thunk_extractor(box2d<double> &box,
                                  render_thunk_list &thunks,
                                  mapnik::feature_impl &feature,
                                  proj_transform const &prj_trans,
                                  renderer_common &common,
                                  box2d<double> const &clipping_extent)
        : box_(box), thunks_(thunks), feature_(feature), prj_trans_(prj_trans),
          common_(common), clipping_extent_(clipping_extent)
    {}

protected:
    box2d<double> &box_;
    render_thunk_list &thunks_;
    mapnik::feature_impl &feature_;
    proj_transform const &prj_trans_;
    renderer_common &common_;
    box2d<double> clipping_extent_;

    void update_box() const
    {
    	label_collision_detector4 &detector = *common_.detector_;

        for (auto const &label : detector)
        {
            if (box_.width() > 0 && box_.height() > 0)
            {
                box_.expand_to_include(label.box);
            }
            else
            {
                box_ = label.box;
            }
        }

        detector.clear();
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

template <typename F, typename T>
void render_group_symbolizer(group_symbolizer const &sym,
                             mapnik::feature_impl &feature,
                             proj_transform const &prj_trans,
                             box2d<double> const &clipping_extent,
                             renderer_common &common,
                             F render_thunks)
{
	typedef T extractor_type;

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

    // create a copied 'virtual' common renderer for processing sub feature symbolizers
    // create an empty detector for it, so we are sure we won't hit anything
    renderer_common virtual_renderer(common);
    virtual_renderer.detector_ = std::make_shared<label_collision_detector4>(common.detector_->extent());

    // keep track of which lists of render thunks correspond to
    // entries in the group_layout_manager.
    std::vector<typename extractor_type::render_thunk_list> layout_thunks;
    size_t num_layout_thunks = 0;

    // layout manager to store and arrange bboxes of matched features
    group_layout_manager layout_manager(props->get_layout(), pixel_position(common.width_ / 2.0, common.height_ / 2.0));

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
        sub_feature->add_geometry(origin_point(prj_trans, common));

        // get the layout for this set of properties
        for (auto const& rule : props->get_rules())
        {
             if (boost::apply_visitor(evaluate<Feature,value_type>(*sub_feature),
                                               *(rule->get_filter())).to_bool())
             {
                // add matched rule and feature to the list of things to draw
                matches.push_back(std::make_pair(rule, sub_feature));

                // construct a bounding box around all symbolizers for the matched rule
                bound_box bounds;
                typename extractor_type::render_thunk_list thunks;
                extractor_type extractor(bounds, thunks, *sub_feature, prj_trans,
                		                 virtual_renderer, clipping_extent);

                for (auto const& sym : *rule)
                {
                    // TODO: construct layout and obtain bounding box
                    boost::apply_visitor(extractor, sym);
                }

                // add the bounding box to the layout manager
                layout_manager.add_member_bound_box(bounds);
                layout_thunks.emplace_back(std::move(thunks));
                ++num_layout_thunks;
                break;
            }
        }
    }

    // determine if we should be tracking repeat distance
    text_placements_ptr placments = get<text_placements_ptr>(sym, keys::text_placements_);
    text_placement_info_ptr placement_info = placments->get_placement_info(common.scale_factor_);
    bool check_repeat = (placement_info->properties.minimum_distance > 0);

    group_symbolizer_helper helper(sym, feature, prj_trans,
                                   common.width_, common.height_,
                                   common.scale_factor_, common.t_,
                                   common.font_manager_, *common.detector_,
                                   clipping_extent);

    for (size_t i = 0; i < matches.size(); ++i)
    {
        if (check_repeat)
        {
            group_rule_ptr match_rule = matches[i].first;
            feature_ptr match_feature = matches[i].second;
            value_unicode_string rpt_key_value = "";

            // get repeat key from matched group rule
            expression_ptr rpt_key_expr = match_rule->get_repeat_key();

            // if no repeat key was defined, use default from group symbolizer
            if (!rpt_key_expr)
            {
                rpt_key_expr = get<expression_ptr>(sym, keys::repeat_key);
            }

            // evalute the repeat key with the matched sub feature if we have one
            if (rpt_key_expr)
            {
                rpt_key_value = boost::apply_visitor(evaluate<Feature,value_type>(*match_feature), *rpt_key_expr).to_unicode();
            }
            helper.add_box_element(layout_manager.offset_box_at(i), rpt_key_value);
        }
        else
        {
            helper.add_box_element(layout_manager.offset_box_at(i));
        }
    }

    placements_list placements = helper.get();

    for (auto const& place : placements)
    {
        const pixel_position &pos = place->get_base_point(); // <-- pixel position given by placement_finder

        for (size_t layout_i = 0; layout_i < num_layout_thunks; ++layout_i)
        {
            const pixel_position &offset = layout_manager.offset_at(layout_i);
            pixel_position render_offset = pos + offset;

            render_thunks(layout_thunks[layout_i], render_offset);
        }
    }
}

} // namespace mapnik

#endif /* MAPNIK_RENDERER_COMMON_PROCESS_GROUP_SYMBOLIZER_HPP */
