#pragma once

void    MTXOpen();

/* ---------------------------------------------------------------------------
    General stuff
--------------------------------------------------------------------------- */

/*/
void    MTXIdentity         ( Mtx m );
void    MTXCopy             ( Mtx src, Mtx dst );
void    MTXConcat           ( Mtx a, Mtx b, Mtx ab );
void    MTXTranspose        ( Mtx src, Mtx xPose );
u32     MTXInverse          ( Mtx src, Mtx inv );
u32     MTXInvXpose         ( Mtx src, Mtx invX );
/*/

// C version
void    C_MTXIdentity       (void);
void    C_MTXCopy           (void);
void    C_MTXConcat         (void);
void    C_MTXTranspose      (void);
void    C_MTXInverse        (void);
void    C_MTXInvXpose       (void);

/* ---------------------------------------------------------------------------
    Matrix-vector
--------------------------------------------------------------------------- */

/*/
void    MTXMultVec          ( Mtx m, VecPtr src, VecPtr dst );
void    MTXMultVecArray     ( Mtx m, VecPtr srcBase, VecPtr dstBase, u32 count );
void    MTXMultVecSR        ( Mtx m, VecPtr src, VecPtr dst );
void    MTXMultVecArraySR   ( Mtx m, VecPtr srcBase, VecPtr dstBase, u32 count );
/*/

/* ---------------------------------------------------------------------------
    Affine matrix math
--------------------------------------------------------------------------- */

/*/
void    MTXQuat             ( Mtx m, QuaternionPtr q );
void    MTXReflect          ( Mtx m, VecPtr p, VecPtr n );
void    MTXTrans            ( Mtx m, f32 xT, f32 yT, f32 zT );
void    MTXTransApply       ( Mtx src, Mtx dst, f32 xT, f32 yT, f32 zT );
void    MTXScale            ( Mtx m, f32 xS, f32 yS, f32 zS );
void    MTXScaleApply       ( Mtx src, Mtx dst, f32 xS, f32 yS, f32 zS );
void    MTXRotRad           ( Mtx m, char axis, f32 rad );
void    MTXRotTrig          ( Mtx m, char axis, f32 sinA, f32 cosA );
void    MTXRotAxisRad       ( Mtx m, VecPtr axis, f32 rad );
/*/

/* ---------------------------------------------------------------------------
    Look at utility
--------------------------------------------------------------------------- */

/*/
void    MTXLookAt           ( Mtx           m, 
                              Point3dPtr    camPos, 
                              VecPtr        camUp, 
                              Point3dPtr    target );
/*/

/* ---------------------------------------------------------------------------
    Project matrix stuff
--------------------------------------------------------------------------- */

/*/
void    MTXFrustum          ( Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f );
void    MTXPerspective      ( Mtx44 m, f32 fovY, f32 aspect, f32 n, f32 f );
void    MTXOrtho            ( Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f );
/*/

/* ---------------------------------------------------------------------------
    Texture projection
--------------------------------------------------------------------------- */

/*/
void    MTXLightFrustum     ( Mtx m, f32 t, f32 b, f32 l, f32 r, f32 n, 
                              f32 scaleS, f32 scaleT, f32 transS, 
                              f32 transT );

void    MTXLightPerspective ( Mtx m, f32 fovY, f32 aspect, f32 scaleS, 
                              f32 scaleT, f32 transS, f32 transT );

void    MTXLightOrtho       ( Mtx m, f32 t, f32 b, f32 l, f32 r, f32 scaleS, 
                              f32 scaleT, f32 transS, f32 transT );
/*/

/* ---------------------------------------------------------------------------
    Vector operations
--------------------------------------------------------------------------- */

/*/
void    VECAdd              ( VecPtr a, VecPtr b, VecPtr ab );
void    VECSubtract         ( VecPtr a, VecPtr b, VecPtr a_b );
void    VECScale            ( VecPtr src, VecPtr dst, f32 scale );
void    VECNormalize        ( VecPtr src, VecPtr unit );
f32     VECSquareMag        ( VecPtr v );
f32     VECMag              ( VecPtr v );
f32     VECDotProduct       ( VecPtr a, VecPtr b );
void    VECCrossProduct     ( VecPtr a, VecPtr b, VecPtr axb );
f32     VECSquareDistance   ( VecPtr a, VecPtr b );
f32     VECDistance         ( VecPtr a, VecPtr b );
void    VECReflect          ( VecPtr src, VecPtr normal, VecPtr dst );
void    VECHalfAngle        ( VecPtr a, VecPtr b, VecPtr half );
/*/

/* ---------------------------------------------------------------------------
    Quaternions
--------------------------------------------------------------------------- */

/*/
void    QUATAdd             ( QuaternionPtr p, QuaternionPtr q, QuaternionPtr r );
void    QUATSubtract        ( QuaternionPtr p, QuaternionPtr q, QuaternionPtr r );
void    QUATMultiply        ( QuaternionPtr p, QuaternionPtr q, QuaternionPtr pq );
void    QUATDivide          ( QuaternionPtr p, QuaternionPtr q, QuaternionPtr r );
void    QUATScale           ( QuaternionPtr q, QuaternionPtr r, f32 scale );
f32     QUATDotProduct      ( QuaternionPtr p, QuaternionPtr q );
void    QUATNormalize       ( QuaternionPtr src, QuaternionPtr unit );
void    QUATInverse         ( QuaternionPtr src, QuaternionPtr inv );
void    QUATExp             ( QuaternionPtr q, QuaternionPtr r );
void    QUATLogN            ( QuaternionPtr q, QuaternionPtr r );
void    QUATMakeClosest     ( QuaternionPtr q, QuaternionPtr qto, QuaternionPtr r );
void    QUATRotAxisRad      ( QuaternionPtr r, VecPtr axis, f32 rad );
void    QUATMtx             ( QuaternionPtr r, Mtx m );
void    QUATLerp            ( QuaternionPtr p, QuaternionPtr q, QuaternionPtr r, f32 t );
void    QUATSlerp           ( QuaternionPtr p, QuaternionPtr q, QuaternionPtr r, f32 t );
void    QUATSquad           ( QuaternionPtr p, QuaternionPtr a, QuaternionPtr b,
                              QuaternionPtr q, QuaternionPtr r, f32 t );
void    QUATCompA           ( QuaternionPtr qprev, QuaternionPtr q,
                              QuaternionPtr qnext, QuaternionPtr a );
/*/
