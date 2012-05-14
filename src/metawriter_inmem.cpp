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

// Mapnik
#include <mapnik/metawriter.hpp>
#include <mapnik/metawriter_inmem.hpp>
#include <mapnik/text_placements/base.hpp>
#include <mapnik/ctrans.hpp>
#include <mapnik/text_path.hpp>

// Boost
#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>

// AGG - for the path-clipping stuff
#include "agg_basics.h"
#include "agg_conv_clip_polyline.h"


using std::map;
using std::string;

namespace {

using mapnik::value;
using mapnik::Feature;
using mapnik::metawriter_properties;

// intersect a set of properties with those in the feature descriptor
map<string,value> intersect_properties(Feature const& feature, metawriter_properties const& properties) {

    map<string,value> nprops;
    BOOST_FOREACH(string p, properties)
    {
        if (feature.has_key(p))
            nprops.insert(std::make_pair(p,feature.get(p)));
    }

    return nprops;
}} // end anonymous namespace

namespace mapnik {

metawriter_inmem::metawriter_inmem(metawriter_properties dflt_properties)
    : metawriter(dflt_properties) {
}

metawriter_inmem::~metawriter_inmem() {
}

void
metawriter_inmem::add_box(box2d<double> const& box, Feature const& feature,
                          CoordTransform const& /*t*/,
                          metawriter_properties const& properties) {
    meta_instance inst;
    inst.box = box;
    inst.properties = intersect_properties(feature, properties);
    instances_.push_back(inst);
}

namespace 
{

void copy_clipped(geometry_container &cont, 
                  Feature const& feature,
                  box2d<double> const& ext,
                  CoordTransform const& t)
{
   typedef coord_transform<CoordTransform,geometry_type> projected_path_type;
   typedef agg::conv_clip_polyline<projected_path_type> path_type;

   for (unsigned int i = 0; i < feature.num_geometries(); ++i)
   {
      // nasty const-cast, but AGG requires a non-const reference as
      // a parameter...
      geometry_type &geom = const_cast<geometry_type &>(feature.get_geometry(i));

      if (geom.num_points() > 1)
      {
         projected_path_type projected(t, geom);
         path_type path(projected);
         path.clip_box(ext.minx(), ext.miny(), ext.maxx(), ext.maxy());
         
         geometry_type *cgeom = new geometry_type(geom.type());
         double x = 0.0, y = 0.0;
         unsigned int command = 0;
         path.rewind(0);

         while (!agg::is_stop(command = path.vertex(&x, &y)))
         {
            cgeom->push_vertex(x, y, CommandType(command));
         }

         cont.push_back(cgeom);
      }
   }
}

} // anonymous namespace

void
metawriter_inmem::add_text(
    boost::ptr_vector<text_path> &placements,
    box2d<double> const& /*extents*/,
    Feature const& feature,
    CoordTransform const& t,
    metawriter_properties const& properties)
{
   // for each placement
   for (size_t n = 0; n < placements.size(); ++n)
   {
      text_path &placement = placements[n];
      box2d<double> bbox;
      bool first = true;

      // gather bbox for this placement
      char_info_ptr c;
      double x, y, angle;
      for (size_t i = 0; i < placement.num_nodes(); ++i) 
      {
         placement.vertex(&c, &x, &y, &angle);

         double x0, y0, x1, y1, x2, y2, x3, y3;
         double sina = sin(angle);
         double cosa = cos(angle);
         x0 = placement.center.x + x - sina*c->ymin;
         y0 = placement.center.y - y - cosa*c->ymin;
         x1 = x0 + c->width * cosa;
         y1 = y0 - c->width * sina;
         x2 = x0 + (c->width * cosa - c->height() * sina);
         y2 = y0 - (c->width * sina + c->height() * cosa);
         x3 = x0 - c->height() * sina;
         y3 = y0 - c->height() * cosa;
         
         if (first)
         {
            bbox.init(x0, y0, x0, y0);
            first = false;
         }
         bbox.expand_to_include(x0, y0);
         bbox.expand_to_include(x1, y1);
         bbox.expand_to_include(x2, y2);
         bbox.expand_to_include(x3, y3);
      }

      // got to put this back to the beginning, or it causes
      // problems for code that uses it later.
      placement.rewind();

      if (bbox.intersects(box2d<double>(0, 0, width_, height_)))
      {
         meta_instance inst;
         inst.properties = intersect_properties(feature, properties);
         inst.box = bbox;
         
         inst.geom_cont = boost::make_shared<geometry_container>();
         copy_clipped(*(inst.geom_cont), feature, bbox, t);
         instances_.push_back(inst);
      }
   }
}

void
metawriter_inmem::add_polygon(path_type & path,
                              Feature const& feature,
                              CoordTransform const& t,
                              metawriter_properties const& properties) {
    add_vertices(path, feature, t, properties);
}

void
metawriter_inmem::add_line(path_type & path,
                           Feature const& feature,
                           CoordTransform const& t,
                           metawriter_properties const& properties) {
    add_vertices(path, feature, t, properties);
}

void
metawriter_inmem::add_vertices(path_type & path,
                               Feature const& feature,
                               CoordTransform const& /*t*/,
                               metawriter_properties const& properties) {
    box2d<double> box;
    unsigned cmd;
    double x = 0.0, y = 0.0;

    path.rewind(0);
    while ((cmd = path.vertex(&x, &y)) != SEG_END) {
        box.expand_to_include(x, y);
    }

    if ((box.width() >= 0.0) && (box.height() >= 0.0)) {
        meta_instance inst;
        inst.properties = intersect_properties(feature, properties);
        inst.box = box;
        instances_.push_back(inst);
    }
}

void
metawriter_inmem::start(metawriter_property_map const& /*properties*/) {
    instances_.clear();
}

const std::list<metawriter_inmem::meta_instance> &
metawriter_inmem::instances() const {
    return instances_;
}

metawriter_inmem::meta_instance_list::const_iterator
metawriter_inmem::inst_begin() const {
    return instances_.begin();
}

metawriter_inmem::meta_instance_list::const_iterator
metawriter_inmem::inst_end() const {
    return instances_.end();
}


}

