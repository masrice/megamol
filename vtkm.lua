--mmCreateView("Project_1","GUIView","::GUIView1")

mmCreateView("Project_1","View3DGL","::View3DGL_1")

mmCreateModule("vtkmDataSource","::Project_1::vtkmDataSource1")
mmCreateModule("TableToADIOS","::Project_1::TableToADIOS1")
mmCreateModule("CSVDataSource","::Project_1::CSVDataSource1")
mmCreateModule("TableToADIOS","::Project_1::TableToADIOS2")
mmCreateModule("CSVDataSource","::Project_1::CSVDataSource2")
mmCreateModule("RenderMDIMesh","::Project_1::RenderMDIMesh1")
mmCreateModule("vtkmStreamLines","::Project_1::vtkmStreamLines1")
mmCreateModule("GPUMeshes","::Project_1::GPUMeshes1")
mmCreateModule("SimpleGPUMtlDataSource","::Project_1::SimpleGPUMtlDataSource1")
mmCreateModule("BoundingBoxRenderer","::Project_1::BoundingBoxRenderer1")
mmCreateModule("vtkmMeshRenderTasks","::Project_1::vtkmMeshRenderTasks1")
--mmCreateModule("View3DGL","::Project_1::View3DGL_1")

mmCreateCall("CallADIOSData","::Project_1::vtkmDataSource1::adiosNodeSlot","::Project_1::TableToADIOS1::adiosSlot")
mmCreateCall("CallADIOSData","::Project_1::vtkmDataSource1::adiosLabelSlot","::Project_1::TableToADIOS2::adiosSlot")
mmCreateCall("TableDataCall","::Project_1::TableToADIOS1::ftSlot","::Project_1::CSVDataSource1::getData")
mmCreateCall("TableDataCall","::Project_1::TableToADIOS2::ftSlot","::Project_1::CSVDataSource2::getData")
mmCreateCall("CallGPURenderTaskData","::Project_1::RenderMDIMesh1::renderTasks","::Project_1::vtkmMeshRenderTasks1::renderTasks")
mmCreateCall("vtkmDataCall","::Project_1::vtkmStreamLines1::vtkCallerSlot","::Project_1::vtkmDataSource1::getdata")
mmCreateCall("CallMesh","::Project_1::GPUMeshes1::meshes","::Project_1::vtkmStreamLines1::meshCalleeSlot")
mmCreateCall("CallRender3DGL","::Project_1::BoundingBoxRenderer1::chainRendering","::Project_1::RenderMDIMesh1::rendering")
mmCreateCall("CallGPUMeshData","::Project_1::vtkmMeshRenderTasks1::gpuMeshes","::Project_1::GPUMeshes1::gpuMeshes")
mmCreateCall("CallGPUMaterialData","::Project_1::vtkmMeshRenderTasks1::gpuMaterials","::Project_1::SimpleGPUMtlDataSource1::gpuMaterials")
--mmCreateCall("CallRenderViewGL","::Project_1::GUIView1::renderview","::Project_1::View3DGL_1::render")
mmCreateCall("CallRender3DGL","::View3DGL_1::rendering","::Project_1::BoundingBoxRenderer1::rendering")

mmSetParamValue("::Project_1::CSVDataSource1::filename",[=[F:/Uni/HiWi/AData/cell_d_50_cubesphere-1.elastic.csv]=])
mmSetParamValue("::Project_1::CSVDataSource1::skipPreface",[=[0]=])
mmSetParamValue("::Project_1::CSVDataSource1::headerNames",[=[true]=])
mmSetParamValue("::Project_1::CSVDataSource1::headerTypes",[=[false]=])
mmSetParamValue("::Project_1::CSVDataSource1::commentPrefix",[=[]=])
mmSetParamValue("::Project_1::CSVDataSource1::colSep",[=[]=])
mmSetParamValue("::Project_1::CSVDataSource1::decSep",[=[Auto]=])
mmSetParamValue("::Project_1::CSVDataSource1::shuffle",[=[false]=])
mmSetParamValue("::Project_1::CSVDataSource2::filename",[=[F:/Uni/HiWi/AData/elementnodelabel.csv]=])
mmSetParamValue("::Project_1::CSVDataSource2::skipPreface",[=[0]=])
mmSetParamValue("::Project_1::CSVDataSource2::headerNames",[=[true]=])
mmSetParamValue("::Project_1::CSVDataSource2::headerTypes",[=[false]=])
mmSetParamValue("::Project_1::CSVDataSource2::commentPrefix",[=[]=])
mmSetParamValue("::Project_1::CSVDataSource2::colSep",[=[]=])
mmSetParamValue("::Project_1::CSVDataSource2::decSep",[=[Auto]=])
mmSetParamValue("::Project_1::CSVDataSource2::shuffle",[=[false]=])
mmSetParamValue("::Project_1::vtkmStreamLines1::fieldName",[=[hs1]=])
mmSetParamValue("::Project_1::vtkmStreamLines1::numSeeds",[=[100]=])
mmSetParamValue("::Project_1::vtkmStreamLines1::stepSize",[=[0.100000]=])
mmSetParamValue("::Project_1::vtkmStreamLines1::numSteps",[=[2000]=])
mmSetParamValue("::Project_1::vtkmStreamLines1::lowerSeedBound",[=[0;0;0]=])
mmSetParamValue("::Project_1::vtkmStreamLines1::upperSeedBound",[=[1;1;1]=])
mmSetParamValue("::Project_1::vtkmStreamLines1::planeMode",[=[normal]=])
mmSetParamValue("::Project_1::vtkmStreamLines1::origin",[=[0;0;0]=])
mmSetParamValue("::Project_1::vtkmStreamLines1::Connection1",[=[-50;-50;0]=])
mmSetParamValue("::Project_1::vtkmStreamLines1::Connection2",[=[50;50;100]=])
mmSetParamValue("::Project_1::vtkmStreamLines1::planeNormal",[=[1;0;0]=])
mmSetParamValue("::Project_1::vtkmStreamLines1::planePoint",[=[0;0;50]=])
mmSetParamValue("::Project_1::vtkmStreamLines1::stpq",[=[0.000000]=])
mmSetParamValue("::Project_1::vtkmStreamLines1::s",[=[0.000000]=])
mmSetParamValue("::Project_1::vtkmStreamLines1::t",[=[0.000000]=])
mmSetParamValue("::Project_1::vtkmStreamLines1::p",[=[0.000000]=])
mmSetParamValue("::Project_1::vtkmStreamLines1::q",[=[0.000000]=])
mmSetParamValue("::Project_1::vtkmStreamLines1::rotateSeedPlane",[=[0.000000]=])
mmSetParamValue("::Project_1::vtkmStreamLines1::planeColor",[=[#800000]=])
mmSetParamValue("::Project_1::vtkmStreamLines1::showGhostPlane",[=[true]=])
mmSetParamValue("::Project_1::SimpleGPUMtlDataSource1::BTF filename",[=[vtkm_streamline]=])
mmSetParamValue("::Project_1::BoundingBoxRenderer1::enableBoundingBox",[=[true]=])
mmSetParamValue("::Project_1::BoundingBoxRenderer1::boundingBoxColor",[=[#ffffff]=])
mmSetParamValue("::Project_1::BoundingBoxRenderer1::smoothLines",[=[true]=])
mmSetParamValue("::Project_1::BoundingBoxRenderer1::enableViewCube",[=[false]=])
mmSetParamValue("::Project_1::BoundingBoxRenderer1::viewCubePosition",[=[top right]=])
mmSetParamValue("::Project_1::BoundingBoxRenderer1::viewCubeSize",[=[100]=])
--mmSetParamValue("::GUIView1::camstore::settings",[=[]=])
--mmSetParamValue("::GUIView1::camstore::overrideSettings",[=[false]=])
--mmSetParamValue("::GUIView1::camstore::autoSaveSettings",[=[false]=])
--mmSetParamValue("::GUIView1::camstore::autoLoadSettings",[=[true]=])
--mmSetParamValue("::GUIView1::resetViewOnBBoxChange",[=[false]=])
--mmSetParamValue("::GUIView1::anim::play",[=[false]=])
--mmSetParamValue("::GUIView1::anim::speed",[=[4.000000]=])
--mmSetParamValue("::GUIView1::anim::time",[=[0.000000]=])
--mmSetParamValue("::GUIView1::backCol",[=[#000020]=])
mmSetParamValue("::View3DGL_1::camstore::settings",[=[]=])
mmSetParamValue("::View3DGL_1::camstore::overrideSettings",[=[false]=])
mmSetParamValue("::View3DGL_1::camstore::autoSaveSettings",[=[false]=])
mmSetParamValue("::View3DGL_1::camstore::autoLoadSettings",[=[true]=])
mmSetParamValue("::View3DGL_1::resetViewOnBBoxChange",[=[false]=])
mmSetParamValue("::View3DGL_1::anim::play",[=[false]=])
mmSetParamValue("::View3DGL_1::anim::speed",[=[4.000000]=])
mmSetParamValue("::View3DGL_1::anim::time",[=[0.000000]=])
mmSetParamValue("::View3DGL_1::backCol",[=[#000020]=])
mmSetParamValue("::View3DGL_1::showLookAt",[=[false]=])
mmSetParamValue("::View3DGL_1::viewKey::MoveStep",[=[0.500000]=])
mmSetParamValue("::View3DGL_1::viewKey::RunFactor",[=[2.000000]=])
mmSetParamValue("::View3DGL_1::viewKey::AngleStep",[=[90.000000]=])
mmSetParamValue("::View3DGL_1::viewKey::FixToWorldUp",[=[true]=])
mmSetParamValue("::View3DGL_1::viewKey::MouseSensitivity",[=[3.000000]=])
mmSetParamValue("::View3DGL_1::viewKey::RotPoint",[=[Look-At]=])
mmSetParamValue("::View3DGL_1::hookOnChange",[=[false]=])
mmSetParamValue("::View3DGL_1::cam::position",[=[0;0;0]=])
mmSetParamValue("::View3DGL_1::cam::orientation",[=[0;0;0;0]=])
mmSetParamValue("::View3DGL_1::cam::projectiontype",[=[Perspective]=])
mmSetParamValue("::View3DGL_1::cam::convergenceplane",[=[0.100000]=])
mmSetParamValue("::View3DGL_1::cam::centeroffset",[=[0;0]=])
mmSetParamValue("::View3DGL_1::cam::halfaperturedegrees",[=[35.000000]=])
mmSetParamValue("::View3DGL_1::cam::halfdisparity",[=[1.000000]=])
mmSetParamValue("::View3DGL_1::cam::ovr::up",[=[0;0;0]=])
mmSetParamValue("::View3DGL_1::cam::ovr::lookat",[=[0;0;0]=])
mmSetParamValue("::View3DGL_1::view::defaultView",[=[Front]=])
mmSetParamValue("::View3DGL_1::view::defaultOrientation",[=[Top]=])
mmSetParamValue("::View3DGL_1::view::cubeOrientation",[=[0;0;0;1]=])
mmSetParamValue("::View3DGL_1::view::showViewCube",[=[false]=])

