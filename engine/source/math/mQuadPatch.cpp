//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "math/mQuadPatch.h"


//******************************************************************************
// Quadratic spline patch
//******************************************************************************
QuadPatch::QuadPatch()
{
    setNumReqControlPoints(3);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void QuadPatch::calcABC(const Point3F* points)
{
    a = points[2] - points[1];
    b = points[1] - points[0];
    c = points[0];
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void QuadPatch::submitControlPoints(SplCtrlPts& points)
{
    Parent::submitControlPoints(points);
    calcABC(points.getPoint(0));
};


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void QuadPatch::setControlPoint(Point3F& point, int index)
{
    ((SplCtrlPts*)getControlPoints())->setPoint(point, index);
    calcABC(getControlPoint(0));
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void QuadPatch::calc(F32 t, Point3F& result)
{
    F32 t2 = t * t;
    result = a * t2 + b * t + c;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void QuadPatch::calc(Point3F* points, F32 t, Point3F& result)
{
    calcABC(points);
    calc(t, result);
}
