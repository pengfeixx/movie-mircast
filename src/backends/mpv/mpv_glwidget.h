/*
 * (c) 2017, Deepin Technology Co., Ltd. <support@deepin.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * is provided AS IS, WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, and
 * NON-INFRINGEMENT.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
#ifndef _DMR_MPV_GLWIDGET_H
#define _DMR_MPV_GLWIDGET_H

#include <QtWidgets>
#include <mpv/render.h>
#include <mpv/render_gl.h>
#include <mpv_proxy.h>
#undef Bool
#include "../../vendor/qthelper.hpp"
#include <DGuiApplicationHelper>
//DWIDGET_USE_NAMESPACE

//add by heyi
typedef void (*mpv_render_contextSet_update_callback)(mpv_render_context *ctx,
                                                      mpv_render_update_fn callback,
                                                      void *callback_ctx);
typedef void (*mpv_render_contextReport_swap)(mpv_render_context *ctx);
typedef void (*mpv_renderContext_free)(mpv_render_context *ctx);
typedef int (*mpv_renderContext_create)(mpv_render_context **res, mpv_handle *mpv,
                                        mpv_render_param *params);
typedef int (*mpv_renderContext_render)(mpv_render_context *ctx, mpv_render_param *params);
typedef uint64_t (*mpv_renderContext_update)(mpv_render_context *ctx);


namespace dmr {
class MpvGLWidget : public QOpenGLWidget
{
    Q_OBJECT
public:
    friend class MpvProxy;

    MpvGLWidget(QWidget *parent, MpvHandle h);
    virtual ~MpvGLWidget();

    /**
     * rounded clipping consumes a lot of resources, and performs bad on 4K video
     */
    void toggleRoundedClip(bool bFalse);
    //add by heyi
    /**
     * @brief setHandle ????????????
     * @param h ???????????????
     */
    void setHandle(MpvHandle h);

protected:
    /**
     * @brief opengl????????? cppcheck??????
     */
    void initializeGL() override;
    void resizeGL(int nWidth, int nHeight) override;
    void paintGL() override;

    void setPlaying(bool);
    void setMiniMode(bool);
    //add by heyi
    /**
     * @brief initMpvFuns ?????????????????????????????????????????????
     */
    void initMpvFuns();
#ifdef __x86_64__
    //?????????????????????????????????
    void updateMovieProgress(qint64 duration, qint64 pos);
#endif
    void setRawFormatFlag(bool bRawFormat);

protected slots:
    void onNewFrame();
    void onFrameSwapped();

private:
    void initMember();
    void updateVbo();
    void updateVboCorners();
    void updateVboBlend();

    void updateMovieFbo();
    void updateCornerMasks();

    void setupBlendPipe();
    void setupIdlePipe();

    void prepareSplashImages();

private:
    MpvHandle m_handle;                //mpv??????
    mpv_render_context *m_pRenderCtx;  //mpv???????????????

    bool m_bPlaying;                   //??????????????????
    bool m_bInMiniMode;                //??????????????????
    bool m_bDoRoundedClipping;         //

    QOpenGLVertexArrayObject m_vao;    //??????????????????
    QOpenGLBuffer m_vbo;               //??????????????????
    QOpenGLTexture *m_pDarkTex;        //????????????????????????
    QOpenGLTexture *m_pLightTex;       //????????????????????????
    QOpenGLShaderProgram *m_pGlProg;

    QOpenGLVertexArrayObject m_vaoBlend;
    QOpenGLBuffer m_vboBlend;
    QOpenGLShaderProgram *m_pGlProgBlend;
    QOpenGLFramebufferObject *m_pFbo;
    QOpenGLShaderProgram *m_pGlProgBlendCorners;

    //textures for corner
    QOpenGLVertexArrayObject m_vaoCorner;
    QOpenGLTexture *m_pCornerMasks[4];
    QOpenGLBuffer m_vboCorners[4];
    QOpenGLShaderProgram *m_pGlProgCorner; //???????????????

    QImage m_imgBgDark;                    //?????????????????????
    QImage m_imgBgLight;                   //?????????????????????

    //add by heyi
    mpv_render_contextSet_update_callback m_callback;
    mpv_render_contextReport_swap m_context_report;
    mpv_renderContext_free m_renderContex;
    mpv_renderContext_create m_renderCreat;
    mpv_renderContext_render m_renderContexRender;
    mpv_renderContext_update m_renderContextUpdate;
#ifdef __x86_64__
    qreal m_pert; //??????????????????
    QString m_strPlayTime; //?????????????????????
#endif
    bool m_bRawFormat;     // ???????????????????????????????????????
};

}

#endif /* ifndef _DMR_MPV_GLWIDGET_H */

