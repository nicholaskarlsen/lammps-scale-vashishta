/*
 *_________________________________________________________________________*
 *      POEMS: PARALLELIZABLE OPEN SOURCE EFFICIENT MULTIBODY SOFTWARE     *
 *      DESCRIPTION: SEE READ-ME                                           *
 *      FILE NAME: fixedpoint.h                                            *
 *      AUTHORS: See Author List                                           *
 *      GRANTS: See Grants List                                            *
 *      COPYRIGHT: (C) 2005 by Authors as listed in Author's List          *
 *      LICENSE: Please see License Agreement                              *
 *      DOWNLOAD: Free at www.rpi.edu/~anderk5                             *
 *      ADMINISTRATOR: Prof. Kurt Anderson                                 *
 *                     Computational Dynamics Lab                          *
 *                     Rensselaer Polytechnic Institute                    *
 *                     110 8th St. Troy NY 12180                           *
 *      CONTACT:        anderk5@rpi.edu                                    *
 *_________________________________________________________________________*/


#ifndef FIXEDPOINT_H
#define FIXEDPOINT_H

#include "point.h"
#include "vect3.h"

class FixedPoint : public Point  {
public:
  FixedPoint();
  ~FixedPoint();
  FixedPoint(double x, double y, double z);
  FixedPoint(Vect3& v);
  PointType GetType();
  Vect3 GetPoint();
  bool ReadInPointData(std::istream& in);
  void WriteOutPointData(std::ostream& out);
};

#endif
