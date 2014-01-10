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
#include <mapnik/text/placement_finder.hpp>
#include <mapnik/text/symbolizer_helpers.hpp>
#include <mapnik/text/layout.hpp>
#include <mapnik/text/renderer.hpp>
#include <mapnik/group/group_layout_manager.hpp>
#include <mapnik/group/group_symbolizer_helper.hpp>

#include <mapnik/geom_util.hpp>
#include <mapnik/symbolizer.hpp>
#include <mapnik/parse_path.hpp>
#include <mapnik/pixel_position.hpp>
#include <mapnik/label_collision_detector.hpp>

#include <mapnik/renderer_common/process_group_symbolizer.hpp>
#include <mapnik/renderer_common/process_point_symbolizer.hpp>
#include <mapnik/text/symbolizer_helpers.hpp>

// agg
#include "agg_trans_affine.h"

// stl
#include <string>

// boost
#include <boost/variant/apply_visitor.hpp>

namespace mapnik {

/**
 * Thunk for rendering a particular instance of a point - this
 * stores all the arguments necessary to re-render this point
 * symbolizer at a later time.
 */
struct agg_point_render_thunk : public common_point_render_thunk
{
    composite_mode_e comp_op_;

    agg_point_render_thunk(pixel_position const &pos, marker const &m,
                           agg::trans_affine const &tr, double opacity,
                           composite_mode_e comp_op)
        : common_point_render_thunk(pos, m, tr, opacity),
          comp_op_(comp_op)
    {}
};

struct agg_text_render_thunk : public common_text_render_thunk
{
    halo_rasterizer_enum halo_rasterizer_;
    composite_mode_e comp_op_;
    
    agg_text_render_thunk(placements_list const &placements,
                          halo_rasterizer_enum halo_rasterizer,
                          composite_mode_e comp_op)
        : common_text_render_thunk(placements),
          halo_rasterizer_(halo_rasterizer), comp_op_(comp_op)
    {}
};

// Variant type for render thunks to allow us to re-render them
// via a static visitor later.
typedef boost::variant<agg_point_render_thunk,
                       agg_text_render_thunk> render_thunk;
typedef std::shared_ptr<render_thunk> render_thunk_ptr;
typedef std::list<render_thunk_ptr> render_thunk_list;

/**
 * Extension of the common render_thunk_collector.
 *
 * Visitor to extract the bounding boxes associated with placing
 * a symbolizer at a fake, virtual point - not real geometry.
 *
 * The bounding boxes can be used for layout, and the thunks are
 * used to re-render at locations according to the group layout.
 */
struct agg_render_thunk_extractor : public boost::static_visitor<>,
                                    public common_render_thunk_extractor<render_thunk>
{
	agg_render_thunk_extractor(box2d<double> &box,
                               render_thunk_list &thunks,
                               mapnik::feature_impl &feature,
                               proj_transform const &prj_trans,
                               renderer_common &common,
                               box2d<double> const &clipping_extent)
        : common_render_thunk_extractor(box, thunks, feature,
                               prj_trans, common, clipping_extent)
    {}

	void operator()(point_symbolizer const &sym) const
    {
        composite_mode_e comp_op = get<composite_mode_e>(sym, keys::comp_op, feature_, src_over);

        render_point_symbolizer(
            sym, feature_, prj_trans_, common_,
            [&](pixel_position const &pos, marker const &marker,
                agg::trans_affine const &tr, double opacity) {
                agg_point_render_thunk thunk(pos, marker, tr, opacity, comp_op);
                thunks_.push_back(std::make_shared<render_thunk>(std::move(thunk)));
            });

        update_box();
    }

	void operator()(text_symbolizer const &sym) const
    {
        box2d<double> clip_box = clipping_extent_;
        text_symbolizer_helper helper(
            sym, feature_, prj_trans_,
            common_.width_, common_.height_,
            common_.scale_factor_,
            common_.t_, common_.font_manager_, *common_.detector_,
            clip_box);

        halo_rasterizer_enum halo_rasterizer = get<halo_rasterizer_enum>(sym, keys::halo_rasterizer, HALO_RASTERIZER_FULL);
        composite_mode_e comp_op = get<composite_mode_e>(sym, keys::comp_op, feature_, src_over);
        
        placements_list const& placements = helper.get();
        agg_text_render_thunk thunk(placements, halo_rasterizer, comp_op);
        thunks_.push_back(std::make_shared<render_thunk>(thunk));

        update_box();
    }

    template <typename T>
    void operator()(T const &) const
    {
        // TODO: warning if unimplemented?
    }
};

/**
 * Render a thunk which was frozen from a previous call to 
 * extract_bboxes. We should now have a new offset at which
 * to render it, and the boxes themselves should already be
 * in the detector from the placement_finder.
 */
struct thunk_renderer : public boost::static_visitor<>
{
    typedef agg_renderer<image_32> renderer_type;
    typedef typename renderer_type::buffer_type buffer_type;
    typedef agg_text_renderer<buffer_type> text_renderer_type;

    thunk_renderer(renderer_type &ren,
                   buffer_type *buf,
                   renderer_common &common,
                   pixel_position const &offset)
        : ren_(ren), buf_(buf), common_(common), offset_(offset)
    {}

    void operator()(agg_point_render_thunk const &thunk) const
    {
        pixel_position new_pos(thunk.pos_.x + offset_.x, thunk.pos_.y + offset_.y);
        ren_.render_marker(new_pos, *thunk.marker_, thunk.tr_, thunk.opacity_,
                           thunk.comp_op_);
    }

    void operator()(agg_text_render_thunk const &thunk) const
    {
        text_renderer_type ren(*buf_, thunk.halo_rasterizer_, thunk.comp_op_,
                               common_.scale_factor_, common_.font_manager_.get_stroker());
        for (glyph_positions_ptr glyphs : thunk.placements_)
        {
            // move the glyphs to the correct offset
            pixel_position const &base_point = glyphs->get_base_point();
            pixel_position new_base_point(base_point.x + offset_.x, base_point.y + offset_.y);
            glyphs->set_base_point(new_base_point);

            // update the position of any marker
            marker_info_ptr marker_info = glyphs->marker();
            if (marker_info)
            {
                pixel_position const &marker_pos = glyphs->marker_pos();
                pixel_position new_marker_pos(marker_pos.x + offset_.x, marker_pos.y + offset_.y);
                glyphs->set_marker(marker_info, new_marker_pos);
            }

            ren.render(*glyphs);
        }
    }

    template <typename T>
    void operator()(T const &) const
    {
        // TODO: warning if unimplemented?
    }

private:
    renderer_type &ren_;
    buffer_type *buf_;
    renderer_common &common_;
    pixel_position offset_;
};

template <typename T0, typename T1>
void agg_renderer<T0,T1>::process(group_symbolizer const& sym,
                                  mapnik::feature_impl & feature,
                                  proj_transform const& prj_trans)
{
    auto render_thunks =
        [&](render_thunk_list const& thunks, pixel_position const& render_offset)
        {
            thunk_renderer ren(*this, current_buffer_, common_, render_offset);
            for (render_thunk_ptr const& thunk : thunks)
            {
                boost::apply_visitor(ren, *thunk);
            }
        };

    render_group_symbolizer<decltype(render_thunks), agg_render_thunk_extractor>(
        sym, feature, prj_trans, clipping_extent(), common_, render_thunks);
}

template void agg_renderer<image_32>::process(group_symbolizer const&,
                                              mapnik::feature_impl &,
                                              proj_transform const&);

}
