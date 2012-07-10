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

#ifndef MAPNIK_GROUP_LAYOUT_MANAGER_HPP
#define MAPNIK_GROUP_LAYOUT_MANAGER_HPP

// mapnik
#include <mapnik/coord.hpp>
#include <mapnik/box2d.hpp>
#include <mapnik/group_layout.hpp>

// boost
#include <boost/foreach.hpp>
#include <boost/variant.hpp>

// std
#include <cmath>

// stl
#include <vector>

using std::vector;

namespace mapnik
{

typedef box2d<double> bound_box;
typedef coord<double,2> layout_offset;

// This visitor will process offsets for the given layout
struct process_layout : public boost::static_visitor<>
{

    // The vector containing the existing, centered item bounding boxes
    const vector<bound_box> &member_boxes_;
    
    // The vector to populate with item offsets
    vector<layout_offset> &member_offsets_;
    
    process_layout(const vector<bound_box> &member_bboxes, 
                 vector<layout_offset> &member_offsets)
       : member_boxes_(member_bboxes), 
         member_offsets_(member_offsets)
    {
    }

    // arrange group memebers in centered, horizontal row
    void operator()(simple_row_layout const& layout) const
    {
        double total_width = (member_boxes_.size() - 1) * layout.get_item_margin();
        BOOST_FOREACH(const bound_box &box, member_boxes_)
        {
            total_width += layout.get_item_margin();
            total_width += box.width();
        }
        
        double x_offset = -(total_width / 2.0);
        BOOST_FOREACH(const bound_box &box, member_boxes_)
        {
            member_offsets_.push_back(layout_offset(x_offset - box.minx(), 0.0));
            x_offset += box.width() + layout.get_item_margin();
        }
    }

    // arrange group members in x horizontal pairs of 2,
    // one to the left and one to the right of center in each pair
    void operator()(pair_layout const& layout) const
    {
        member_offsets_.resize(member_boxes_.size());
        double y_margin = layout.get_item_margin();
        double x_margin = y_margin / 2.0;

        if (member_boxes_.size() == 1)
        {
            member_offsets_[0] = layout_offset(0, 0);
            return;
        }

        bound_box layout_box;
        int middle_ifirst = (member_boxes_.size() - 1) >> 1, top_i = 0, bottom_i = 0;
        if (middle_ifirst % 2 == 0)
        {
            layout_box = make_horiz_pair(0, 0.0, 0, x_margin, layout.get_max_difference());
            top_i = middle_ifirst - 2;
            bottom_i = middle_ifirst + 2;
        }
        else
        {
            top_i = middle_ifirst - 1;
            bottom_i = middle_ifirst + 1;
        }

        while (bottom_i >= 0 && top_i < member_offsets_.size())
        {
            layout_box.expand_to_include(make_horiz_pair(top_i, layout_box.miny() - y_margin, -1, x_margin, layout.get_max_difference()));
            layout_box.expand_to_include(make_horiz_pair(bottom_i, layout_box.maxy() + y_margin, 1, x_margin, layout.get_max_difference()));
            top_i -= 2;
            bottom_i += 2;
        }

    }
  
private:

    // Place member bound boxes at [ifirst] and [ifirst + 1] in a horizontal pairi, vertically
    //   align with pair_y, store corresponding offsets, and return bound box of combined pair
    // Note: x_margin is the distance between box edge and x center
    bound_box make_horiz_pair(size_t ifirst, double pair_y, int y_dir, double x_margin, double max_diff) const
    {
        // two boxes available for pair
        if (ifirst + 1 < member_boxes_.size())
        {
            double x_center = member_boxes_[ifirst].width() - member_boxes_[ifirst + 1].width();
            if (max_diff < 0.0 || std::abs(x_center) <= max_diff)
            {
                x_center = 0.0;
            }

            bound_box pair_box = box_offset_align(ifirst, x_center - x_margin, pair_y, -1, y_dir);
            pair_box.expand_to_include(box_offset_align(ifirst + 1, x_center + x_margin, pair_y, 1, y_dir));
            return pair_box;
        }
        
        // only one box available for this "pair", so keep x-centered and handle y-offset
        return box_offset_align(ifirst, 0, pair_y, 0, y_dir);
    }


    // Offsets member bound box at [i] and align with (x, y), in direction <x_dir, y_dir>
    // stores corresponding offset, and returns modified bounding box
    bound_box box_offset_align(size_t i, double x, double y, int x_dir, int y_dir) const
    {
        const bound_box &box = member_boxes_[i];
        layout_offset offset((x_dir == 0 ? x : x - (x_dir < 0 ? box.maxx() : box.minx())),
                             (y_dir == 0 ? y : y - (y_dir < 0 ? box.maxy() : box.miny())));

        member_offsets_[i] = offset;
        return bound_box(box.minx() + offset.x, box.miny() + offset.y, box.maxx() + offset.x, box.maxy() + offset.y);
    }
};

struct MAPNIK_DECL group_layout_manager
{    
    group_layout_manager(const group_layout &layout)
        : layout_(layout),
          member_boxes_(vector<bound_box>()),
          member_offsets_(vector<layout_offset>()),
          update_layout_(true)
    {
    }
    
    group_layout_manager(const group_layout &layout, const vector<bound_box> &item_boxes)
        : layout_(layout),
          member_boxes_(item_boxes),
          member_offsets_(vector<layout_offset>()),
          update_layout_(true)
    {
    }
    
    void set_layout(const group_layout &layout)
    {
        layout_ = layout;
        update_layout_ = true;
    }
    
    void add_member_bound_box(const bound_box &member_box)
    {
        member_boxes_.push_back(member_box);
        update_layout_ = true;
    }
    
    const layout_offset &offset_at(size_t i)
    {
        handle_update();
        return member_offsets_.at(i);
    }
    
    bound_box offset_box_at(size_t i)
    {
        handle_update();
        const layout_offset &offset = member_offsets_.at(i);
        const bound_box &box = member_boxes_.at(i);
        return box2d<double>(box.minx() + offset.x, box.miny() + offset.y,
                             box.maxx() + offset.x, box.maxy() + offset.y);
    }
    
private:

    void handle_update()
    {
        if (update_layout_)
        {
            member_offsets_.clear();
            boost::apply_visitor(process_layout(member_boxes_, member_offsets_), layout_);
            update_layout_ = false;
        }     
    }

    group_layout layout_;
    vector<bound_box> member_boxes_;
    vector<layout_offset> member_offsets_;
    bool update_layout_;
};

}   // namespace mapnik

#endif // MAPNIK_GROUP_LAYOUT_MANAGER_HPP