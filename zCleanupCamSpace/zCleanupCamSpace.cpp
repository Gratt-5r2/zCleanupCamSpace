// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
  static Array<zCVisual*> CleanupModelList;
  static Array<zCVob*> CleanupVobListIn;
  static Array<zCVob*> CleanupVobListOut;

  static void ResetCleanupModelList() {
    CleanupModelList.Clear();
    CleanupVobListIn.Clear();
    CleanupVobListOut.Clear();
  }

  static bool NeedToCleanupThisVob( zCVob* vob ) {
    if( !vob || !vob->visual )
      return false;

    return CleanupModelList.HasEqualSorted( vob->visual );
  }

  static Array<string> ReadStringArray( const string& section, const string& parameter, const string& default = "" ) {
    string source = zoptions->ReadString( Z section, Z parameter, default );
    Array<string> array = source.Split( "," );
    for( uint i = 0; i < array.GetNum(); i++ )
      array[i].Shrink().Upper();

    return array;
  }

  static void CheckSoftModelInList( zCVisual* visual, Array<zCVisual*>& list ) {
    if( !list.HasEqualSorted( visual ) )
      list.InsertSorted( visual );
  }

  static void CheckSoftModel( zCVisual* visual ) {
    if( !visual )
      return;

    static Array<string> cleanupList = ReadStringArray( "zCleanupCamSpace", "cleanupList", "*smalltree*, *tanne*, *bush*, *cupressus*, *vinemaple*, *caveweb*, *farn*, *hohetanne*" );

    string visualName = visual->GetVisualName().Upper();
    if( visualName.IsEmpty() || visualName.EndWith( ".PFX" ) )
      return;

    for( uint i = 0; i < cleanupList.GetNum(); i++ )
      if( visualName.CompareMasked( cleanupList[i] ) )
        CheckSoftModelInList( visual, CleanupModelList );
  }

  HOOK Hook_zCVob_SetVisual PATCH( &zCVob::SetVisual, &zCVob::SetVisual_Union );

  void zCVob::SetVisual_Union( zCVisual* visual ) {
    THISCALL( Hook_zCVob_SetVisual )(visual);
    CheckSoftModel( visual );
  }

  static bool FastTraceRayVob( zCVob* vob, const zVEC3& from, const zVEC3& vec ) {
    static zTTraceRayReport traceRayReport;
    return vob->TraceRay( from, vec, zTRACERAY_FIRSTHIT, traceRayReport ) != False;
  }

  static bool FastTraceRayVob( zCVob* vob, const zVEC3& from, const zVEC3& vec, const zVEC3& right ) {
    float vectorsLength      = 50.0f;
    zVEC3 atVector           = vec;
    zVEC3 rightVector        = right;
    atVector.Normalize()    *= vectorsLength;
    rightVector.Normalize() *= vectorsLength;

    return FastTraceRayVob( vob, from, vec + atVector );
  }

  static bool FastTraceRayVob( zCVob* vob ) {
    zVEC3 start  = ogame->GetCameraVob()->GetPositionWorld();
    zVEC3 end    = player->GetPositionWorld() + zVEC3( 0.0f, 40.0f, 0.0f );
    zVEC3 vector = end - start;
    if( FastTraceRayVob( vob, start, vector * 1.2f ) )
      return true;

    zCVob* focusVob = player->GetFocusNpc();
    if( !focusVob )
      return false;

    end = focusVob->GetPositionWorld();
    if( start.Distance( end ) > 1800.0f )
      return false;

    vector = end - start;
    if( vector.Length() > 1000.0f )
      vector.Normalize() *= 1000.0f;

    return FastTraceRayVob( vob, start, vector * 1.2f );
  }

  static void CheckIntersections() {
    static Array<zCVob*> vobCache;
    static Array<zCVob*> vobIdle;

    zVEC3 camPos        = ogame->GetCameraVob()->GetPositionWorld();
    zVEC3 rightVector   = ogame->GetCameraVob()->GetRightVectorWorld();
    zVEC3 playerPos     = player->GetPositionWorld() + zVEC3( 0.0f, 35.0f, 0.0f );
    zVEC3 vector        = (playerPos - camPos);
    oCWorld* world      = ogame->GetGameWorld();
    auto& renderVobList = world->bspTree.renderVobList;

    vobCache.Clear();
    for( int i = 0; i < renderVobList.GetNum(); i++ ) {
      zCVob* vob = renderVobList[i];
      if( NeedToCleanupThisVob( vob ) )
        if( FastTraceRayVob( vob ) ) {
          vobCache += vob;
        }
    }

    vobIdle  = CleanupVobListIn;
    vobIdle ^= vobCache;

    for( uint i = 0; i < vobIdle.GetNum(); i++ ) {
      zCVob* vob = vobIdle[i];
      if( !FastTraceRayVob( vob ) ) {
        CleanupVobListIn ^= vob;
        CleanupVobListOut |= vob;
      }
    }

    CleanupVobListIn |= vobCache;
  }

  static void UpdateAlphaChannel() {
    for( uint i = 0; i < CleanupVobListIn.GetNum(); i++ ) {
      zCVob* vob = CleanupVobListIn[i];
      if( vob->visualAlpha > 0.2 ) {
        vob->visualAlphaEnabled = True;
        vob->visualAlpha *= 0.9;
      }
    }

    for( uint i = 0; i < CleanupVobListOut.GetNum(); i++ ) {
      zCVob* vob = CleanupVobListOut[i];
      if( vob->visualAlpha > 0.95f ) {
        vob->visualAlpha = 1.0;
        vob->visualAlphaEnabled = False;
        CleanupVobListOut.Remove( vob );
      }
      else
        vob->visualAlpha *= 1.1;
    }
  }

  static CPatchBool* CreatePatchVariable( const string& name ) {
    if( !CHECK_THIS_ENGINE )
      return Null;

    CPatchBool* boolean = new CPatchBool();
    boolean->Init();
    boolean->SetObjectName( name );
    boolean->SetValue( True );
    return boolean;
  }

  static CPatchBool* traceRayTransp = CreatePatchVariable( "traceRayTransp" );

  static void CheckCamSpace() {
    traceRayTransp->SetValue( True );
    CheckIntersections();
    UpdateAlphaChannel();
    traceRayTransp->SetValue( False );
  }
}