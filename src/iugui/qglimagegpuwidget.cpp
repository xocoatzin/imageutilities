/*
 * Copyright (c) ICG. All rights reserved.
 *
 * Institute for Computer Graphics and Vision
 * Graz University of Technology / Austria
 *
 *
 * This software is distributed WITHOUT ANY WARRANTY; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the above copyright notices for more information.
 *
 *
 * Project     : ImageUtilities
 * Module      : GUI
 * Class       : QGLImageGpuWidget
 * Language    : C++
 * Description : Definition of a QGLWidget rendering GPU memory (2D)
 *
 * Author     : Manuel Werlberger
 * EMail      : werlberger@icg.tugraz.at
 *
 */

#include <QMouseEvent>
#include <QMenu>
#include <QSignalMapper>
#include <QWidgetAction>
#include <QCheckBox>
#include <GL/glew.h>
#include <cuda_runtime.h>
#include <iumath.h>
#include "qglimagegpuwidget.h"

namespace iuprivate {
extern IuStatus cuCopyImageToPbo(iu::Image* image,
                                 unsigned int num_channels, unsigned int bit_depth,
                                 uchar4 *dst,
                                 float min=0.0f, float max=1.0f);
extern IuStatus cuCopyOverlayToPbo(iuprivate::Overlay* overlay, uchar4 *dst, IuSize size);
}

namespace iu {

//-----------------------------------------------------------------------------
QGLImageGpuWidget::QGLImageGpuWidget(QWidget *parent) :
  QGLWidget(parent),
  gl_pbo_(NULL),
  gl_tex_(NULL),
  cuda_pbo_resource_(NULL),
  image_(0),
  num_channels_(0),
  bit_depth_(0),
  normalize_(false),
  min_(0.0f),
  max_(1.0f),
  init_ok_(false),
  zoom_(1.0f),
  mouse_x_old_(0),
  mouse_y_old_(0),
  mouse_x_(0),
  mouse_y_(0)
{
  //updateGL();/ // invoke OpenGL initialization
  this->initializeGL();

  IuStatus status = iu::checkCudaErrorState();
  if (status == IU_NO_ERROR)
    printf("QGLImageGpuWidget::QGLImageGpuWidget: initialized (widget + opengl).\n");
  else
    fprintf(stderr,"QGLImageGpuWidget::QGLImageGpuWidget: error while init (widget + opengl).\n");


  context_menu_ = new QMenu("Widget properties", this);
  this->createActions();
}

//-----------------------------------------------------------------------------
QGLImageGpuWidget::~QGLImageGpuWidget()
{
  while(!overlay_list_.isEmpty())
    delete(overlay_list_.takeFirst());
}

//-----------------------------------------------------------------------------
void QGLImageGpuWidget::createActions()
{
  // close event with ctrl-w
  action_close_ = new QAction(tr("Close Widget"), this);
  action_close_->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_W));
  connect(action_close_, SIGNAL(triggered()), this, SLOT(close()));
  this->addAction(action_close_);
  context_menu_->addAction(action_close_);

  // signal mapping for overlays
  overlay_signal_mapper_ = new QSignalMapper(this);
  connect(overlay_signal_mapper_, SIGNAL(mapped(const QString&)),
          this, SLOT(slotActivateOverlay(const QString&)));

//  // zoom
//  action_zoom_group_ = new QMenu(tr("Zoom"),this);
//  action_zoom_group_->setStatusTip(tr("Selecte fixed zoom"));
//  context_menu_->addMenu(action_zoom_group_);

//  action_zoom_0p25_ = new QAction(tr("Zoom 1:4"), this);
//  action_zoom_0p25_->setStatusTip(tr("Set zoom to 0.25"));
//  connect(action_zoom_0p25_, SIGNAL(triggered()), this, SLOT(slotZoom0p25()));
//  action_zoom_group_->addAction(action_zoom_0p25_);
//  action_zoom_0p33_ = new QAction(tr("Zoom 1:3"), this);
//  action_zoom_0p33_->setStatusTip(tr("Set zoom to 0.33"));
//  connect(action_zoom_0p33_, SIGNAL(triggered()), this, SLOT(slotZoom0p33()));
//  action_zoom_group_->addAction(action_zoom_0p33_);
//  action_zoom_0p5_ = new QAction(tr("Zoom 1:2"), this);
//  action_zoom_0p5_->setStatusTip(tr("Set zoom to 0.5"));
//  connect(action_zoom_0p5_, SIGNAL(triggered()), this, SLOT(slotZoom0p5()));
//  action_zoom_group_->addAction(action_zoom_0p5_);
//  action_zoom_reset_ = new QAction(tr("Zoom 1:1"), this);
//  action_zoom_reset_->setShortcut(tr("Ctrl+0"));
//  action_zoom_reset_->setStatusTip(tr("Reset zoom to original image size"));
//  connect(action_zoom_reset_, SIGNAL(triggered()), this, SLOT(slotZoomReset()));
//  action_zoom_group_->addAction(action_zoom_reset_);
//  action_zoom_2_ = new QAction( tr("Zoom 2:1"), this);
//  action_zoom_2_->setStatusTip(tr("Set zoom to 2"));
//  connect(action_zoom_2_, SIGNAL(triggered()), this, SLOT(slotZoom2()));
//  action_zoom_group_->addAction(action_zoom_2_);
//  action_zoom_3_ = new QAction( tr("Zoom 3:1"), this);
//  action_zoom_3_->setStatusTip(tr("Set zoom to 3"));
//  connect(action_zoom_3_, SIGNAL(triggered()), this, SLOT(slotZoom3()));
//  action_zoom_group_->addAction(action_zoom_3_);
//  action_zoom_4_ = new QAction(tr("Zoom 4:1"), this);
//  action_zoom_4_->setStatusTip(tr("Set zoom to 4"));
//  connect(action_zoom_4_, SIGNAL(triggered()), this, SLOT(slotZoom4()));
//  action_zoom_group_->addAction(action_zoom_4_);
}

/* ****************************************************************************
     Input
 * ***************************************************************************/


//-----------------------------------------------------------------------------
void QGLImageGpuWidget::setImage(iu::ImageGpu_8u_C1 *image, bool normalize)
{
  printf("QGLImageGpuWidget::setImage(ImageGpu_8u_C1*)\n");

  if(image == 0)
  {
    fprintf(stderr, "The given input image is null!\n");
  }

  // FIXMEEE
  // TODO cleanup pbo and texture if we have already an image set

  if(image_ != 0)
  {
    if(image->size() == image_->size())
    {
      printf("set new image with same sizings\n");
      image_ = image;
      return;
    }
    else
    {
      printf("currently we do not support setting another image with different size.\n");
    }
  }

  image_ = image;
  num_channels_ = 1;
  bit_depth_ = 8;
  if (!this->init())
  {
    fprintf(stderr, "Failed to initialize OpenGL buffers.\n");
    init_ok_ = false;
  }
  else
    init_ok_ = true;

  if(init_ok_)
    this->resize(image_->width(), image_->height());
}

//-----------------------------------------------------------------------------
void QGLImageGpuWidget::setImage(iu::ImageGpu_8u_C4 *image, bool normalize)
{
  printf("QGLImageGpuWidget::setImage(ImageGpu_8u_C4*)\n");

  if(image == 0)
  {
    fprintf(stderr, "The given input image is null!\n");
  }

  // FIXMEEE
  // TODO cleanup pbo and texture if we have already an image set

  if(image_ != 0)
  {
    if(image->size() == image_->size())
    {
      printf("set new image with same sizings\n");
      image_ = image;
      return;
    }
    else
    {
      printf("currently we do not support setting another image with different size.\n");
    }
  }

  image_ = image;
  num_channels_ = 4;
  bit_depth_ = 8;
  if (!this->init())
  {
    fprintf(stderr, "Failed to initialize OpenGL buffers.\n");
    init_ok_ = false;
  }
  else
    init_ok_ = true;

  if(init_ok_)
    this->resize(image_->width(), image_->height());
}

//-----------------------------------------------------------------------------
void QGLImageGpuWidget::setImage(iu::ImageGpu_32f_C1 *image, bool normalize)
{
  printf("QGLImageGpuWidget::setImage(ImageGpu_32f_C1*)\n");

  if(image == 0)
  {
    fprintf(stderr, "The given input image is null!\n");
  }

  // FIXMEEE
  // TODO cleanup pbo and texture if we have already an image set

  if(image_ != 0)
  {
    if(image->size() == image_->size())
    {
      printf("set new image with same sizings\n");
      image_ = image;
      normalize_ = normalize;
      return;
    }
    else
    {
      printf("currently we do not support setting another image with different size.\n");
    }
  }

  image_ = image;
  num_channels_ = 1;
  bit_depth_ = 32;
  normalize_ = normalize;
  if (!this->init())
  {
    fprintf(stderr, "Failed to initialize OpenGL buffers.\n");
    init_ok_ = false;
  }
  else
    init_ok_ = true;

  if(init_ok_)
    this->resize(image_->width(), image_->height());
  this->update();
}

//-----------------------------------------------------------------------------
void QGLImageGpuWidget::setImage(iu::ImageGpu_32f_C4 *image, bool normalize)
{
  printf("QGLImageGpuWidget::setImage(ImageGpu_32f_C4*)\n");

  if(image == 0)
  {
    fprintf(stderr, "The given input image is null!\n");
  }

  // FIXMEEE
  // TODO cleanup pbo and texture if we have already an image set

  if(image_ != 0)
  {
    if(image->size() == image_->size())
    {
      printf("set new image with same sizings\n");
      image_ = image;
      return;
    }
    else
    {
      printf("currently we do not support setting another image with different size.\n");
    }
  }

  image_ = image;
  num_channels_ = 4;
  bit_depth_ = 32;
  normalize_ = normalize;
  if (!this->init())
  {
    fprintf(stderr, "Failed to initialize OpenGL buffers.\n");
    init_ok_ = false;
  }
  else
    init_ok_ = true;

  if(init_ok_)
    this->resize(image_->width(), image_->height());
  this->update();
}

/* ****************************************************************************
     some interaction
 * ***************************************************************************/

//-----------------------------------------------------------------------------
void QGLImageGpuWidget::setMinMax(float min, float max)
{
  min_ = min;
  max_ = max;
}

//-----------------------------------------------------------------------------
void QGLImageGpuWidget::autoMinMax()
{
  if(bit_depth_ == 8)
  {
    if(num_channels_ == 1)
    {
      iu::ImageGpu_8u_C1* img = reinterpret_cast<iu::ImageGpu_8u_C1*>(image_);
      if(img == 0) return;
      unsigned char cur_min, cur_max;
      iu::minMax(img, img->roi(), cur_min, cur_max);
      min_ = static_cast<float>(cur_min);
      max_ = static_cast<float>(cur_max);
    }
    else
    {
      iu::ImageGpu_8u_C4* img = reinterpret_cast<iu::ImageGpu_8u_C4*>(image_);
      if(img == 0) return;
      uchar4 cur_min, cur_max;
      iu::minMax(img, img->roi(), cur_min, cur_max);
      min_ = static_cast<float>(IUMIN(IUMIN(cur_min.x, cur_min.y), cur_min.z));
      max_ = static_cast<float>(IUMAX(IUMAX(cur_max.x, cur_max.y), cur_max.z));
    }
  }
  else
  {
    if(num_channels_ == 1)
    {
      iu::ImageGpu_32f_C1* img = reinterpret_cast<iu::ImageGpu_32f_C1*>(image_);
      if(img == 0) return;
      iu::minMax(img, img->roi(), min_, max_);
    }
    else
    {
      iu::ImageGpu_32f_C4* img = reinterpret_cast<iu::ImageGpu_32f_C4*>(image_);
      if(img == 0) return;
    }
  }
}

//-----------------------------------------------------------------------------
void QGLImageGpuWidget::setAutoNormalize(bool flag)
{
  normalize_ = flag;
}

//-----------------------------------------------------------------------------
void QGLImageGpuWidget::addOverlay(QString name, iu::Image* constraint_image,
                iu::LinearMemory* lut_values, iu::LinearDeviceMemory_8u_C4* lut_colors,
                bool active)
{
  if(constraint_image->roi() != image_->roi())
    qFatal("Size (ROI) of rendered image and overlay constraint image do not match.");

  iuprivate::Overlay* overlay = new iuprivate::Overlay(name, constraint_image,
                                                       lut_values, lut_colors, active);
  overlay_list_.append(overlay);

  // create action for overlay
  if(overlay_list_.empty())
    context_menu_->addSeparator()->setText(tr("Overlays:"));

  // create actions for context menu switches
  QCheckBox *overlay_check_box = new QCheckBox(name, this);
  overlay_check_box->setChecked(active);
  QWidgetAction *action_overlay_widget = new QWidgetAction(this);
  action_overlay_widget->setDefaultWidget(overlay_check_box);
  action_list_overlays_.append(action_overlay_widget);
  connect(overlay_check_box, SIGNAL(toggled(bool)), overlay_signal_mapper_, SLOT(map()));
  overlay_signal_mapper_->setMapping(overlay_check_box, name);
  context_menu_->addAction(action_overlay_widget);
}

/* ****************************************************************************
     interactive
 * ***************************************************************************/

//-----------------------------------------------------------------------------
void QGLImageGpuWidget::slotActivateOverlay(const QString& overlay_name)
{
  // search for the signal sender in the overlay list
  iuprivate::OverlayList::iterator it;
  for ( it=overlay_list_.begin() ; it != overlay_list_.end(); it++ )
  {
    if(overlay_name == (*it)->getName())
    {
      // toggle overlay state if the sender was found
      (*it)->toogleActive();
      this->update();
    }
  }
}

//-----------------------------------------------------------------------------
void QGLImageGpuWidget::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton)
  {
    printf("QGLWidget: mouse pressed %d/%d\n", event->x(), event->y());
    // Save current Position
    int offset_y = IUMAX(0, static_cast<int>(round(image_->height()/zoom_)) - image_->height());
    mouse_x_old_ = floor(event->x()/zoom_);
    mouse_y_old_ = floor(event->y()/zoom_) - offset_y;

    emit mousePressed(mouse_x_old_, mouse_y_old_);
    emit mousePressed(mouse_x_old_, mouse_y_old_, event->globalX(), event->globalY());
  }
}

//-----------------------------------------------------------------------------
void QGLImageGpuWidget::mouseReleaseEvent(QMouseEvent * event)
{
  int offset_y = IUMAX(0, static_cast<int>(round(image_->height()/zoom_)) - image_->height());
  emit mouseReleased(floor(event->x()/zoom_), floor(event->y()/zoom_) - offset_y);
}

//-----------------------------------------------------------------------------
void QGLImageGpuWidget::mouseMoveEvent(QMouseEvent * event)
{
  int offset_y = IUMAX(0, static_cast<int>(round(image_->height()/zoom_)) - image_->height());
  mouse_x_ = floor(event->x()/zoom_);
  mouse_y_ = floor(event->y()/zoom_) - offset_y;
  emit mouseMoved(mouse_x_old_, mouse_y_old_, mouse_x_, mouse_y_);
  mouse_x_old_ = mouse_x_;
  mouse_y_old_ = mouse_y_;

}

//--------------------------------------------------------------------------------
void QGLImageGpuWidget::wheelEvent(QWheelEvent *event)
{
  int num_degrees = event->delta() / 8;
  int num_steps = num_degrees / 15;

//  if (event->orientation() == Qt::Vertical && QApplication::keyboardModifiers() == Qt::ControlModifier)
//  {
    float cur_zoom = zoom_ + float(num_steps)/30.0f;
    this->resize(image_->width()*cur_zoom, image_->height()*cur_zoom);
    event->accept();
//  }
//  else
//    event->ignore();
}

//-----------------------------------------------------------------------------
void QGLImageGpuWidget::contextMenuEvent(QContextMenuEvent* event)
{
  context_menu_->exec(event->globalPos());
}

/* ****************************************************************************
     GL stuff
 * ***************************************************************************/

//-----------------------------------------------------------------------------
void QGLImageGpuWidget::initializeGL()
{
  printf("QGLImageGpuWidget::initializeGL()\n");

  makeCurrent();

  glewInit();
  printf("  Loading extensions: %s\n", glewGetErrorString(glewInit()));
  if (!glewIsSupported( "GL_VERSION_1_5 GL_ARB_vertex_buffer_object GL_ARB_pixel_buffer_object" ))
  {
    fprintf(stderr, "QGLImageGpuWidget Error: failed to get minimal GL extensions for QGLImageGpuWidget.\n");
    fprintf(stderr, "The widget requires:\n");
    fprintf(stderr, "  OpenGL version 1.5\n");
    fprintf(stderr, "  GL_ARB_vertex_buffer_object\n");
    fprintf(stderr, "  GL_ARB_pixel_buffer_object\n");
    fflush(stderr);
    return;
  }

  printf("QGLImageGpuWidget::initializeGL() done\n");
}

//-----------------------------------------------------------------------------
void QGLImageGpuWidget::createTexture()
{
  if(gl_tex_) deleteTexture();

  IuSize sz = image_->size();

  // Enable Texturing
  glEnable(GL_TEXTURE_2D);

  // Generate a texture identifier
  glGenTextures(1,&gl_tex_);

  // Make this the current texture (remember that GL is state-based)
  glBindTexture(GL_TEXTURE_2D, gl_tex_);

  // Allocate the texture memory. The last parameter is NULL since we only
  // want to allocate memory, not initialize it
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, sz.width, sz.height, 0, GL_BGRA,
               GL_UNSIGNED_BYTE, NULL);
#if 0
  // Must set the filter mode, GL_LINEAR enables interpolation when scaling
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  // Note: GL_TEXTURE_RECTANGLE_ARB may be used instead of
  // GL_TEXTURE_2D for improved performance if linear interpolation is
  // not desired. Replace GL_LINEAR with GL_NEAREST in the
  // glTexParameteri() call
#else
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#endif
}

//-----------------------------------------------------------------------------
void QGLImageGpuWidget::deleteTexture()
{
  if(gl_tex_)
  {
    glDeleteTextures(1, &gl_tex_);
    gl_tex_ = NULL;
  }
}

//-----------------------------------------------------------------------------
void QGLImageGpuWidget::createPbo()
{
  IuSize sz = image_->size();

  // set up vertex data parameter
  int num_texels = sz.width * sz.height;
  int num_values = num_texels * 4;
  int size_tex_data = sizeof(GLubyte) * num_values;

  // Generate a buffer ID called a PBO (Pixel Buffer Object)
  glGenBuffers(1,&gl_pbo_);
  // Make this the current UNPACK buffer (OpenGL is state-based)
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_pbo_);
  // Allocate data for the buffer. 4-channel 8-bit image
  glBufferData(GL_PIXEL_UNPACK_BUFFER, size_tex_data, NULL, GL_DYNAMIC_COPY);

  cudaGraphicsGLRegisterBuffer( &cuda_pbo_resource_, gl_pbo_, cudaGraphicsMapFlagsNone );
}

void QGLImageGpuWidget::deletePbo()
{
  if(gl_pbo_)
  {
    // delete the PBO
    cudaGraphicsUnregisterResource(cuda_pbo_resource_);
    glDeleteBuffers( 1, &gl_pbo_ );
    gl_pbo_=NULL;
    cuda_pbo_resource_ = NULL;
  }
}

//-----------------------------------------------------------------------------
bool QGLImageGpuWidget::init()
{
  printf("QGLImageGpuWidget::init()\n");


  this->createTexture();

  if (iu::checkCudaErrorState() != IU_NO_ERROR)
    fprintf(stderr, "error while initializing texture (gl)\n");
  else
    printf("  Texture created.\n");

  this->createPbo();

  if (iu::checkCudaErrorState() != IU_NO_ERROR)
    fprintf(stderr, "error while initializing pbo (gl)\n");
  else
    printf("  PBO created.\n");

  return !iu::checkCudaErrorState();
}


//-----------------------------------------------------------------------------
void QGLImageGpuWidget::resizeGL(int w, int h)
{
  if(image_ == 0)
    return;

  float zoom_w = float(w)/float(image_->width());
  float zoom_h = float(h)/float(image_->height());
  zoom_ = (zoom_w < zoom_h) ? zoom_w : zoom_h;
}

//-----------------------------------------------------------------------------
void QGLImageGpuWidget::paintGL()
{
  if(image_ == 0)
    return;

  //  printf("QGLImageGpuWidget::paintGL()\n");

  // map GL <-> CUDA resource
  uchar4 *d_dst = NULL;
  size_t start;
  cudaGraphicsMapResources(1, &cuda_pbo_resource_, 0);
  cudaGraphicsResourceGetMappedPointer((void**)&d_dst, &start, cuda_pbo_resource_);

  // check for min/max values if normalization is activated
  if(normalize_)
    this->autoMinMax();

  // get image data
  iuprivate::cuCopyImageToPbo(image_, num_channels_, bit_depth_, d_dst, min_, max_);
  cudaThreadSynchronize();

  iuprivate::OverlayList::iterator it;
  for ( it=overlay_list_.begin() ; it != overlay_list_.end(); it++ )
  {
    if ((*it)->isActive())
    {
      cuCopyOverlayToPbo((*it), d_dst, image_->size());
    }
  }
  cudaThreadSynchronize();

  // unmap GL <-> CUDA resource
  cudaGraphicsUnmapResources(1, &cuda_pbo_resource_, 0);

  // common display code path
  {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_pbo_);
    glBindTexture(GL_TEXTURE_2D, gl_tex_ );
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image_->width(), image_->height(),
                    GL_RGBA, GL_UNSIGNED_BYTE, NULL );

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity ();
    glOrtho (-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);

    glViewport(0, 0, (int)floor(image_->width()*zoom_), (int)floor(image_->height()*zoom_));

#if 0
    glBegin(GL_TRIANGLES);
    glTexCoord2f(0, 0); glVertex2f(-1, 1);
    glTexCoord2f(2, 0); glVertex2f( 3, 1);
    glTexCoord2f(0, 2); glVertex2f(-1,-3);
    glEnd();
#else
    glBegin(GL_QUADS);
    glTexCoord2f( 0.0, 0.0); glVertex3f(-1.0,  1.0, 0.5);
    glTexCoord2f( 1.0, 0.0); glVertex3f( 1.0,  1.0, 0.5);
    glTexCoord2f( 1.0, 1.0); glVertex3f( 1.0, -1.0, 0.5);
    glTexCoord2f( 0.0, 1.0); glVertex3f(-1.0, -1.0, 0.5);
    glEnd ();
#endif
  }
  glPopMatrix();

  //  printf("QGLImageGpuWidget::paintGL() done\n");

}


} // namespace iu

