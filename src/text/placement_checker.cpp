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
//mapnik
#include <mapnik/debug.hpp>
#include <mapnik/label_collision_detector.hpp>
#include <mapnik/ctrans.hpp>
#include <mapnik/expression_evaluator.hpp>
#include <mapnik/text/placement_checker.hpp>
#include <mapnik/text/layout.hpp>
#include <mapnik/text/text_properties.hpp>
#include <mapnik/text/placements_list.hpp>
#include <mapnik/text/vertex_cache.hpp>

// agg
#include "agg_conv_clip_polyline.h"

// stl
#include <vector>

namespace mapnik
{

// Output is centered around (0,0)
static void rotated_box2d(box2d<double> & box, rotation const& rot, double width, double height)
{
    double new_width = width * rot.cos + height * rot.sin;
    double new_height = width * rot.sin + height * rot.cos;
    box.init(-new_width/2., -new_height/2., new_width/2., new_height/2.);
}

placement_checker::placement_checker(text_layout const &layout,
                                     text_placement_info_ptr info,
                                     rotation const &orientation,
                                     justify_alignment_e jalign,
                                     bool has_marker,
                                     double scale_factor,
                                     box2d<double> const &extent,
                                     marker_info_ptr marker,
                                     pixel_position const &pos,
                                     pixel_position const &alignment_offset,
                                     pixel_position const &marker_displacement,
                                     bool marker_unlocked,
                                     box2d<double> const &marker_box)
    : layout_(layout),
      info_(info),
      orientation_(orientation),
      jalign_(jalign),
      has_marker_(has_marker),
      scale_factor_(scale_factor),
      extent_(extent),
      marker_(marker)
{
    /* Find text origin. */
    pixel_position displacement = scale_factor_ * info_->properties.displacement + alignment_offset;
    if (info_->properties.rotate_displacement) displacement = displacement.rotate(!orientation_);
    base_point_ = pos + displacement;
    
    rotated_box2d(text_bbox_, orientation, layout_.width(), layout_.height());
    text_bbox_.re_center(base_point_.x, base_point_.y);
    
    if (has_marker_)
    {
        marker_real_pos_ = (marker_unlocked ? pos : base_point_) + marker_displacement;
        marker_bbox_ = marker_box;
        marker_bbox_.move(marker_real_pos_.x, marker_real_pos_.y);
    }
}

bool placement_checker::has_placement(DetectorType &detector) const
{
    /* For point placements it is faster to just check the bounding box. */
    if (layout_.num_lines() && collision(detector, text_bbox_)) return false;
    /* add_marker first checks for collision and then updates the detector.*/
    if (has_marker_ && collision(detector, marker_bbox_)) return false;
    
    return true;
}

void placement_checker::add_to_detector(DetectorType &detector) const
{
    if (layout_.num_lines()) detector.insert(text_bbox_, layout_.text());
    if (has_marker_) detector.insert(marker_bbox_);
}

glyph_positions_ptr placement_checker::get_positions() const
{
    glyph_positions_ptr glyphs = std::make_shared<glyph_positions>();
    glyphs->set_base_point(base_point_);
    
    if (has_marker_)
    {
        glyphs->set_marker(marker_, marker_real_pos_);
    }
    
    /* IMPORTANT NOTE:
       x and y are relative to the center of the text
       coordinate system:
       x: grows from left to right
       y: grows from bottom to top (opposite of normal computer graphics)
    */
    double x, y;
    
    // set for upper left corner of text envelope for the first line, top left of first character
    y = layout_.height() / 2.0;
    glyphs->reserve(layout_.glyphs_count());
    
    for ( auto const& line : layout_)
    {
        y -= line.height(); //Automatically handles first line differently
        x = jalign_offset(line.width());
        
        for (auto const& glyph : line)
        {
            // place the character relative to the center of the string envelope
            glyphs->push_back(glyph, pixel_position(x, y).rotate(orientation_), orientation_);
            if (glyph.width)
            {
                //Only advance if glyph is not part of a multiple glyph sequence
                x += glyph.width + glyph.format->character_spacing * scale_factor_;
            }
        }
    }
    
    return glyphs;
}

double placement_checker::jalign_offset(double line_width) const //TODO
{
    if (jalign_ == J_MIDDLE) return -(line_width / 2.0);
    if (jalign_ == J_LEFT)   return -(layout_.width() / 2.0);
    if (jalign_ == J_RIGHT)  return (layout_.width() / 2.0) - line_width;
    return 0;
}

bool placement_checker::collision(DetectorType &detector, const box2d<double> &box) const
{
    if (!detector.extent().intersects(box)
        ||
        (info_->properties.avoid_edges && !extent_.contains(box))
        ||
        (info_->properties.minimum_padding > 0 &&
         !extent_.contains(box + (scale_factor_ * info_->properties.minimum_padding)))
        ||
        (!info_->properties.allow_overlap &&
         !detector.has_point_placement(box, info_->properties.minimum_distance * scale_factor_))
        )
    {
        return true;
    }
    return false;
}

} // ns mapnik
