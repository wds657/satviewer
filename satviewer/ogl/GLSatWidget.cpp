/*
 * GLSatWidget.cpp
 *
 *  Created on: 15.05.2009
 *      Author: Yan Coduemat
 */

#include "GLSatWidget.h"

#include <QDir>
#include <QPainter>
#include <QMouseEvent>
#include <QDateTime>
#include <QFontDialog>
#include <QFileDialog>
#include <QColorDialog>
//#include <QtOpenGL>
#include <QFont>

#include "math.h"
#include "satogl.h"

GLSatWidget::GLSatWidget(QWidget *parent) : GLSatAbstractWidget(parent) {
    ui.setupUi(settingsWidget);
    initSatOgl();

    connect(ui.btnColor     , SIGNAL(clicked()), this, SLOT(btnColorClicked    ()));
    connect(ui.btnFontNet   , SIGNAL(clicked()), this, SLOT(btnFontNetClicked  ()));
    connect(ui.btnMapFile   , SIGNAL(clicked()), this, SLOT(onBtnMapFileClicked()));
    connect(ui.comboBoxColor, SIGNAL(currentIndexChanged(int)), this, SLOT(onColorTypeChanged(int)));
    connect(ui.checkSun     , SIGNAL(clicked(bool)), this, SLOT(onCheckSun  (bool)));
    connect(ui.checkNight   , SIGNAL(clicked(bool)), this, SLOT(onCheckNight(bool)));
    connect(ui.spinZoom, SIGNAL(valueChanged(double)), this, SLOT(changeZoom(double)));
    connect(ui.spinX   , SIGNAL(valueChanged(double)), this, SLOT(changeX   (double)));
    connect(ui.spinY   , SIGNAL(valueChanged(double)), this, SLOT(changeY   (double)));
}

GLSatWidget::~GLSatWidget() {

}

void GLSatWidget::initSatOgl() {
    for (int i = 0; i < VertexCount; ++i) {
        vertex[i][0] = 0;
        vertex[i][1] = 0;
    }
}

void GLSatWidget::glZoneLines(float lat) {
	uint8_t type = 0;
	int breakpoint[2];
	float polar;
	glShadeModel(GL_SMOOTH);
	glEnable(GL_ALPHA_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);

    if (fabs(vertex[0][0] - vertex[VertexCount - 1][0]) > 1) {
    	breakpoint[type] = 0;
		type++;
	}
	for (int i = 0; i < VertexCount - 1; i++)
		if (fabs(vertex[i][0] - vertex[i + 1][0]) > 1) {
			breakpoint[type] = i + 1;
			type++;
		}

	if (lat < 0) polar = 1.0f;
	else polar = -1.0f;
	switch (type) {

		case 0:
		glBegin(GL_LINE_STRIP);
			for (int i = 0; i < VertexCount; i++) glVertex2fv(vertex[i]);
			glVertex2fv(vertex[0]);
		glEnd();
		break;

		case 1:
			glBegin(GL_LINE_STRIP);
				glVertex2f(polar, vertex[breakpoint[0]][1]);
				for (int i = breakpoint[0]; i < VertexCount; i++) {
					glVertex2fv(vertex[i]);
				}
				for (int i = 0; i < breakpoint[0]; i++) {
					glVertex2fv(vertex[i]);
				}
				glVertex2f(-polar, vertex[breakpoint[0]][1]);
			glEnd();
		break;

		case 2:
			if (vertex[breakpoint[0]][0] < 0) polar = -1;
			else polar = 1;
			glBegin(GL_LINE_STRIP);
				glVertex2f(polar, vertex[breakpoint[0]][1]);
				for (int i = breakpoint[0] + 1; i < breakpoint[1] - 1; i++) glVertex2fv(vertex[i]);
				glVertex2f(polar, vertex[breakpoint[1] - 1][1]);
			glEnd();

			if (vertex[breakpoint[1]][0] < 0) polar = -1;
			else polar = 1;
			glBegin(GL_LINE_LOOP);
				glVertex2f(polar, vertex[breakpoint[1]][1]);
				for (int i = breakpoint[1] + 1; i < VertexCount; i++) glVertex2fv(vertex[i]);
				if (breakpoint[0] > 0) {
					for (int i = 0; i < breakpoint[0] - 1; i++) glVertex2fv(vertex[i]);
					glVertex2f(polar, vertex[breakpoint[0] - 1][1]);
				}
				else glVertex2f(polar, vertex[VertexCount - 1][1]);
			glEnd();
		break;
		}
	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
}

void GLSatWidget::glZoneNight(float lat) {
	int breakpoint = 0;
	float polar;

	if (fabs(vertex[0][0] - vertex[VertexCount - 1][0]) > 1) {
		breakpoint = 0;
	}
	for (int i = 0; i < VertexCount - 1; i++)
		if (fabs(vertex[i][0] - vertex[i + 1][0]) > 1) {
			breakpoint = i + 1;
		}

	if (lat > 0) polar = 1.0f;
	else polar = -1.0f;

	glBegin(GL_TRIANGLE_STRIP);
		glVertex2f(-polar, vertex[breakpoint][1]);
		glVertex2f(-polar, polar);
		for (int i = breakpoint; i < VertexCount; i++) {
			glVertex2fv(vertex[i]);
			glVertex2f(vertex[i][0], polar);
		}
		for (int i = 0; i < breakpoint; i++) {
			glVertex2fv(vertex[i]);
			glVertex2f(vertex[i][0], polar);
		}
		glVertex2f(polar, vertex[breakpoint][1]);
		glVertex2f(polar, polar);
	glEnd();
}

void GLSatWidget::lfi_ort(double fi, double lam, double* xyz) {
    xyz[0] = cos(fi) * cos(lam);
    xyz[1] = cos(fi) * sin(lam);
    xyz[2] = sin(fi);
}

bool GLSatWidget::testZRV(double* crd1, double* crd2, double fiz) {
    double fi = 2*asin(0.5*sqrt(pow(crd2[0]-crd1[0], 2) +
                                pow(crd2[1]-crd1[1], 2) +
                                pow(crd2[2]-crd1[2], 2)));
    if (fabs(fi) > fiz) return false; else return true;
}

int GLSatWidget::testIOZRV(Satellite *sat, Location *loc, ZrvIoList *list, double &time) {
    int result = -1;
    if (time == 0) return result;  // o_O
    bool inZRV;
    double fiz = 0;
    double z = sat->zrvWidth();
    //---------------------
    if (fabs(cos(z)*sat->radiusEarth()) > (sat->height() + sat->radiusEarth())) fiz = -z;
    else fiz = (M_PI/2 - z - asin(cos(z)*sat->radiusEarth()/(sat->height() + sat->radiusEarth())));
    //---------------------
    double ortl[3], orts[3];
    lfi_ort(loc->latitude()*M_PI/180, loc->longitude()*M_PI/180, ortl);
    lfi_ort(sat->latitude(), sat->longitude(), orts);
    inZRV = testZRV(orts, ortl, fiz);
    if (inZRV) result = 0;

    int ioIndex = list->find(sat, loc);
    if (inZRV && (ioIndex == -1)) {
                    result = 1;
                    list->add(sat, loc, time);
    }
    else if (!inZRV && (ioIndex != -1)) {
            result = 2;
            time = list->item(ioIndex)->time;
            list->del(ioIndex);
    }
    /**/
    return result;
}

void GLSatWidget::compileZrl(Location *loc) {
	if (!(loc->isVisibleZrv() | loc->isVisibleLines())) return;

	double const deg2rad = M_PI/180;
	double z = loc->zrlRange()*deg2rad;
	double a1 = M_PI -loc->zrlAzimuth()*deg2rad;
	double a2 = loc->zrlWidth()*deg2rad;
	double b = -loc->latitude()*deg2rad;
	double l = loc->longitude()*deg2rad + M_PI;

	int cntRadius = VertexCount/4;
	int cntFront = VertexCount - 2*cntRadius;
	if (a2 >= 2*M_PI) {
		cntFront = VertexCount;
		cntRadius = 0;
	}

	double fiz = 0;
	for (int i = 0; i < cntRadius; i++) {
		double y0 = asin(cos(fiz)*sin(b) + sin(fiz)*cos(b)*cos(a1 - a2/2));
		double x0 = l + asin(sin(fiz)*sin(a1 - a2/2)/cos(y0));
		if ((cos(fiz) - sin(b)*sin(y0)) < 0 ) x0 = 2*l - x0 + M_PI;
		x0 = fmod(x0/M_PI + 4, 2) - 1;
		y0 = 2*y0/M_PI;
		vertex[i][0] = x0;
		vertex[i][1] = y0;
		fiz += z/cntRadius;
	}

	double a = a1 - a2/2;
	for (int i = 0; i < cntFront; i++) {
		double y0 = asin(cos(z)*sin(b) + sin(z)*cos(b)*cos(a));
		double x0 = l + asin(sin(z)*sin(a)/cos(y0));
		if ((cos(z) - sin(b)*sin(y0)) < 0 ) x0 = 2*l - x0 + M_PI;
		x0 = fmod(x0/M_PI + 4, 2) - 1;
		y0 = 2*y0/M_PI;
		vertex[i + cntRadius][0] = x0;
		vertex[i + cntRadius][1] = y0;
		a += a2/cntFront;
	}

	fiz = z;
	for (int i = 0; i < cntRadius; i++) {
		double y0 = asin(cos(fiz)*sin(b) + sin(fiz)*cos(b)*cos(a1 + a2/2));
		double x0 = l + asin(sin(fiz)*sin(a1 + a2/2)/cos(y0));
		if ((cos(fiz) - sin(b)*sin(y0)) < 0 ) x0 = 2*l - x0 + M_PI;
		x0 = fmod(x0/M_PI + 4, 2) - 1;
		y0 = 2*y0/M_PI;
		vertex[i + cntFront + cntRadius][0] = x0;
		vertex[i + cntFront + cntRadius][1] = y0;
		fiz -= z/cntRadius;
	}

	GLuint clr = loc->colorZrv();
	glColor4ubv((GLubyte *)&clr);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	if (loc->isVisibleZrv()) gluZone(loc->latitude()*M_PI/180.0);
	glDisable(GL_BLEND);
	if (loc->isVisibleLines()) {
		clr = loc->colorLines();
		glColor4ubv((GLubyte *)&clr);
	    glLineWidth(loc->linesWidth());
	    glZoneLines(loc->latitude()*M_PI/180.0);
	}
}

bool GLSatWidget::testShadow(Satellite *sat, Satellite *sun) {
	if ((sat == 0) || (sun == 0)) return true;
	double r_sat = sqrt(sat->xyz()[0]*sat->xyz()[0] + sat->xyz()[1]*sat->xyz()[1] + sat->xyz()[2]*sat->xyz()[2]);
	double r_sun = sqrt(sun->xyz()[0]*sun->xyz()[0] + sun->xyz()[1]*sun->xyz()[1] + sun->xyz()[2]*sun->xyz()[2]);
	if ((r_sat == 0) || (r_sun == 0) || (sat->radiusEarth() > r_sat)) return true;
	double f = asin(sat->radiusEarth()/r_sat);
	double l = M_PI - acos((sat->xyz()[0]*sun->xyz()[0] + sat->xyz()[1]*sun->xyz()[1] + sat->xyz()[2]*sun->xyz()[2])/(r_sat*r_sun));
	if ((-f < l) && (l < f)) return false;
	return true;
}

void GLSatWidget::gluZone(float lat) {
//    uint8_t type = 0;
//    int breakpoint[2];
//    float polar;
//    double point[5][3];
//
//    if (fabs(vertex[0][0] - vertex[VertexCount - 1][0]) > 1) {
//            breakpoint[type] = 0;
//            type++;
//    }
//    for (int i = 0; i < VertexCount - 1; i++) {
//            if (fabs(vertex[i][0] - vertex[i + 1][0]) > 1) {
//                    breakpoint[type] = i + 1;
//                    type++;
//            }
//    }
//    if (lat < 0) polar = 1.0f;
//    else polar = -1.0f;
//
//    GLUtesselator *tess = gluNewTess();
//    gluTessProperty(tess, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_ODD);
//    gluTessProperty(tess, GLU_TESS_BOUNDARY_ONLY, GL_FALSE);
//    gluTessProperty(tess, GLU_TESS_TOLERANCE, 0);
//
//    /* GLUtessEndProc - |>--+ it's shit */
//    gluTessCallback(tess, GLU_TESS_BEGIN , (GLvoid (CALLBACK *)())glBegin    );
//    gluTessCallback(tess, GLU_TESS_END   , (GLvoid (CALLBACK *)())glEnd      );
//    gluTessCallback(tess, GLU_TESS_VERTEX, (GLvoid (CALLBACK *)())glVertex2dv);
//
//    gluTessBeginPolygon(tess, 0);
//    gluTessBeginContour(tess);
//
//    GLdouble dvertex[VertexCount][3];
//    for (int i = 0; i < VertexCount; i++) {
//            dvertex[i][0] = vertex[i][0];
//            dvertex[i][1] = vertex[i][1];
//            dvertex[i][2] = 0;
//    }
//
//    switch (type) {
//
//            case 0:
//    //		glBegin(GL_POLYGON);
//    //			for (int i = 0; i < VertexCount; i++) glVertex2fv(vertex[i]);
//    //		glEnd();
//                    for (int i = 0; i < VertexCount; i++)
//                            gluTessVertex(tess, dvertex[i], dvertex[i]);
//            break;
//
//            case 1:
//                    point[0][0] = -polar;
//                    point[0][1] = vertex[breakpoint[0]][1];
//                    point[0][2] = 0;
//                    point[1][0] = -polar;
//                    point[1][1] = polar;
//                    point[1][2] = 0;
//
//                    point[2][0] = polar;
//                    point[2][1] = polar;
//                    point[2][2] = 0;
//                    point[3][0] = polar;
//                    point[3][1] = vertex[breakpoint[0]][1];
//                    point[3][2] = 0;
//
//                    for (int i = 0; i < 4; i++)
//                            gluTessVertex(tess, point[i], point[i]);
//
//                    for (int i = breakpoint[0]; i < VertexCount; i++) {
//                            gluTessVertex(tess, dvertex[i], dvertex[i]);
//                    }
//
//                    for (int i = 0; i < breakpoint[0]; i++) {
//                            gluTessVertex(tess, dvertex[i], dvertex[i]);
//                    }
//
//            break;
//
//            case 2:
//                    if (vertex[breakpoint[0]][0] < 0) polar = -1;
//                    else polar = 1;
//                    point[0][0] = polar;
//                    point[0][1] = dvertex[breakpoint[0]][1];
//                    point[0][2] = 0;
//                    gluTessVertex(tess, point[0], point[0]);
//                    for (int i = breakpoint[0] + 1; i < breakpoint[1] - 1; i++)
//                            gluTessVertex(tess, dvertex[i], dvertex[i]);
//                    point[1][0] = polar;
//                    point[1][1] = vertex[breakpoint[1] - 1][1];
//                    point[1][2] = 0;
//                    gluTessVertex(tess, point[1], point[1]);
//                    gluTessEndContour(tess);
//                    gluTessBeginContour(tess);
//
//                    if (vertex[breakpoint[1]][0] < 0) polar = -1;
//                    else polar = 1;
//                    point[2][0] = polar;
//                    point[2][1] = dvertex[breakpoint[1]][1];
//                    point[2][2] = 0;
//                    gluTessVertex(tess, point[2], point[2]);
//                    point[3][0] = polar;
//                    point[3][1] = dvertex[breakpoint[0] - 1][1];
//                    point[3][2] = 0;
//                    point[4][0] = polar;
//                    point[4][1] = dvertex[VertexCount - 1][1];
//                    point[4][2] = 0;
//                            for (int i = breakpoint[1] + 1; i < VertexCount; i++)
//                                    gluTessVertex(tess, dvertex[i], dvertex[i]);
//                            if (breakpoint[0] > 0) {
//                                    for (int i = 0; i < breakpoint[0] - 1; i++)
//                                            gluTessVertex(tess, dvertex[i], dvertex[i]);
//                                    gluTessVertex(tess, point[3], point[3]);
//                            }
//                            else gluTessVertex(tess, point[4], point[4]);
//            break;
//    }
//    gluTessEndContour(tess);
//    gluTessEndPolygon(tess);
//    gluDeleteTess(tess);
}

void GLSatWidget::compileZRV(Satellite *sat, bool poly, bool lines, uint32_t colorPoly, uint32_t colorLines) {
	double fiz = 0;
	double z = sat->zrvWidth();
	if (fabs(cos(z)*sat->radiusEarth()) > (sat->height() + sat->radiusEarth())) fiz = -z;
	else fiz = (M_PI/2 - z - asin(cos(z)*sat->radiusEarth()/(sat->height() + sat->radiusEarth())));
	double x, y;
	double polar, angle = 0;
	if (fabs(sat->latitude()) + fiz < M_PI/2) polar = 1;
	else polar = -1;
	for (int i = 0; i < VertexCount; i++) {
		y = asin(cos(fiz)*sin(sat->latitude()) + sin(fiz)*cos(sat->latitude())*cos(angle));
		x = sin(fiz)*sin(angle)/cos(y);
		if (x > 1) x = 1.0;
		if (x < -1) x = -1.0;
		x = sat->longitude() + polar*asin(x);
		if ((cos(fiz) - sin(sat->latitude())*sin(y)) < 0 ) {x = 2*sat->longitude() - x + M_PI;}
		x = fmod(x + M_PI, 2*M_PI);
		if (x < 0) x += 2*M_PI;
		vertex[i][0] = (float)(x/M_PI - 1);
		vertex[i][1] = (float)(-2*y/M_PI);
		angle += 2*M_PI/(double)VertexCount;
	}
	if (poly) {
		glColor4ubv((uint8_t *)&colorPoly);
	    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
//		glZone(sat->latitude());
		gluZone(sat->latitude());
		glDisable(GL_BLEND);
	}

	if (lines){
		glColor4ubv((uint8_t *)&colorLines);
	    glLineWidth(sat->linesWidth());
		glZoneLines(sat->latitude());
	}
}

void GLSatWidget::readSettings() {

    QDir dir = QDir::home();
    QSettings::setUserIniPath(dir.path());
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "satviewer", "satviewer");
    settings.setIniCodec("UTF-8");

    if (!isValid()) {
        return;
    }
    
    QFont font;
    font.fromString( settings.value("cylindrical/fontNet", QFont().toString()).toString() );
    ui.btnFontNet->setFont(font);
    setFontNet(font);

    setColorNet    (settings.value("cylindrical/colorNet"    , 0x20606060).toUInt());
    setColorNetFont(settings.value("cylindrical/colorNetFont", 0x0       ).toUInt());
    setColorNight  (settings.value("cylindrical/colorNight"  , 0x40000000).toUInt());
    ui.comboBoxColor->setCurrentIndex(1);
    ui.comboBoxColor->setCurrentIndex(0);
    QString filePath = settings.value("cylindrical/mapFile", "").toString();
    loadTexture(filePath);
    ui.btnMapFile->setText(filePath);

    ui.checkSun->setChecked(settings.value("cylindrical/checkSun", true).toBool());
    showSun(ui.checkSun->isChecked());
    ui.checkNight->setChecked(settings.value("cylindrical/checkNight", true).toBool());
    showNight(ui.checkNight->isChecked());

    ui.spinZoom->setValue(settings.value("cylindrical/mapZoom", 1).toDouble());
    ui.spinX->setValue   (settings.value("cylindrical/mapX"   , 0).toDouble());
    ui.spinY->setValue   (settings.value("cylindrical/mapY"   , 0).toDouble());
}

void GLSatWidget::writeSettings(QSettings *settings) {
    settings->setValue("cylindrical/mapFile", ui.btnMapFile->text());
    settings->setValue("cylindrical/fontNet", fontNet().toString ());

    settings->setValue("cylindrical/colorNet"    , colorNet    ());
    settings->setValue("cylindrical/colorNetFont", colorNetFont());
    settings->setValue("cylindrical/colorNight"  , colorNight  ());

    settings->setValue("cylindrical/checkSun"  , ui.checkSun->isChecked()  );
    settings->setValue("cylindrical/checkNight", ui.checkNight->isChecked());

    settings->setValue("cylindrical/mapZoom", ui.spinZoom->value());
    settings->setValue("cylindrical/mapX"   , ui.spinX->value   ());
    settings->setValue("cylindrical/mapY"   , ui.spinY->value   ());
}

void GLSatWidget::addSat(Satellite *sat) {
    GLSatAbstractWidget::addSat(sat);
    if (sat->satWObject == 0) setIcon(sat);
}

void GLSatWidget::addLoc(Location* loc) {
    GLSatAbstractWidget::addLoc(loc);
    if (loc->satWObject == 0) setIcon(loc);
}

void GLSatWidget::setIcon(Satellite *sat, QString fileName) {
    if (sat->satWObject != 0) delete sat->satWObject;
    if (!fileName.isEmpty()) sat->setIcon(fileName);
    sat->satWObject = new GLSprite(sat->iconName(), this);
}

void GLSatWidget::setIcon(Location *loc, QString fileName) {
    if (loc->satWObject != 0) delete loc->satWObject;
    if (!fileName.isEmpty()) loc->setIcon(fileName);
    loc->satWObject = new GLSprite(loc->iconName(), this);
}

void GLSatWidget::btnColorClicked() {
    QPalette pal = ui.btnColor->palette();
    QColor color = QColorDialog::getColor(pal.color(QPalette::Button), this, 0, QColorDialog::ShowAlphaChannel);
    if (!color.isValid()) return;

    pal.setColor(QPalette::Button, color);
    ui.btnColor->setPalette(pal);
    QRgb rgb = color.rgba();

    //OpenGL 0xAABBGGRR //Qt 0xAARRGGBB
    uint8_t a, r, g, b;
    b = rgb;
    g = rgb >> 8;
    r = rgb >> 16;
    a = rgb >> 24;
    rgb = (a << 24) | (b << 16) | (g << 8) | (r);

    switch (ui.comboBoxColor->currentIndex()) {
        case 0:
            setColorNet(rgb);
        break;
        case 1:
            setColorNetFont(rgb);
        break;
        case 2:
            setColorNight(rgb);
        break;
    }
}

void GLSatWidget::btnFontNetClicked() {
    bool ok;
    QFont font = QFontDialog::getFont(&ok, ui.btnFontNet->font(), this);
    if (ok) {
        ui.btnFontNet->setFont(font);
        setFontNet(font);
    }
    puts(ui.btnFontNet->font().toString().toLocal8Bit().data());
}

void GLSatWidget::onColorTypeChanged(int index) {
    QPalette pal = ui.btnColor->palette();
    QRgb rgb = pal.color(QPalette::Button).rgba();

    switch (index) {
        case 0:
            rgb = colorNet();
        break;
        case 1:
            rgb = colorNetFont();
        break;
        case 2:
            rgb = colorNight();
        break;
    }

    //OpenGL 0xAABBGGRR //Qt 0xAARRGGBB
    uint8_t a, r, g, b;
    r = rgb;
    g = rgb >> 8;
    b = rgb >> 16;
    a = rgb >> 24;
    rgb = (a << 24) | (r << 16) | (g << 8) | (b);

    pal.setColor(QPalette::Button, QColor::fromRgb(rgb));
    ui.btnColor->setPalette(pal);
}

void GLSatWidget::onBtnMapFileClicked() {
    QDir dir = QDir::home();
    dir.cd("satviewer");
    QString filePath = QFileDialog::getOpenFileName(this, "Open Map File", dir.path(), "PNG Files (*.png)");
    if (filePath == "") return;
    loadTexture(filePath);
    ui.btnMapFile->setText(filePath);
}

void GLSatWidget::onCheckSun(bool value) {
    showSun(value);
}

void GLSatWidget::onCheckNight(bool value) {
    showNight(value);
}

void GLSatWidget::initializeGL() {
    GLSatAbstractWidget::initializeGL();
    QDir dir = QDir::home();
    dir.cd("satviewer/icons");
    sprite_current.load(dir.filePath("current.png"), this);
    sprite_active.load(dir.filePath("active.png"), this);
//    sun->satWObject = new GLSprite(dir.filePath("sun.png"), this);
}

void GLSatWidget::compileMapList() {
    
    glNewList(list_map, GL_COMPILE);
        if (textureID) {
            textureID->bind();
        }

        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        glEnable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);
            glTexCoord2f(0.0, 0.0); glVertex2f(-1.0, -1.0);
            glTexCoord2f(0.0, 1.0); glVertex2f(-1.0,  1.0);
            glTexCoord2f(1.0, 1.0); glVertex2f( 1.0,  1.0);
            glTexCoord2f(1.0, 0.0); glVertex2f( 1.0, -1.0);
        glEnd();
        glDisable(GL_TEXTURE_2D);
    glEndList();

    glNewList(list_net, GL_COMPILE);
        glLineWidth(1.0);
        glBlendFunc(GL_SRC_COLOR, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_BLEND);
        glColor4ubv((uint8_t *)&clrNet);

        glBegin(GL_LINES);
            for (float i = -1.0; i < 1.0; i += 1.0/12.0) {
                glVertex2f(i, -1);
                glVertex2f(i,  1);
            }
            for (float i = -1 + 1.0/6.0; i < 1; i = i + 1.0/6.0) {
                glVertex2f(-1.0, i);
                glVertex2f( 1.0, i);
            }
        glEnd();

        glDisable(GL_LINE_STIPPLE);
        glColor4ub(192, 64, 64, 192);

        glBegin(GL_LINES);
            glVertex2f(-1, 0);
            glVertex2f( 1, 0);
        glEnd();

        //The equator, 23 1/2 degrees north of the Tropic of Capricorn,
        //and 23 1/2 degrees south of the Tropic of Cancer.
        glColor4ubv((uint8_t *)&clrNet);
        glEnable(GL_LINE_STIPPLE);
        glLineStipple(1, 0xF0F0);
        glBegin(GL_LINES);
            glVertex2f(-1, -23.5/90.0);
            glVertex2f( 1, -23.5/90.0);
            glVertex2f(-1,  23.5/90.0);
            glVertex2f( 1,  23.5/90.0);
        glEnd();
        glDisable(GL_LINE_STIPPLE);
        glDisable(GL_BLEND);
    glEndList();

    glNewList(list_labels, GL_COMPILE);
        float tmp = 0;
        float borderW = 1 - 2.0*(float)fntNet.pointSize()/(float)width();
        float borderH = 1 - 2.0*(float)fntNet.pointSize()/(float)height();
        //		  glLoadIdentity();
        glColor4ubv((uint8_t *)&clrNet);
        for (int i = 1; i < 6; i++) {
            renderText((float)(-1 + (1 - borderW)/4.0), (float)(i/3.0 - 1), QString().number(abs(i*30 - 90)), clrNetFont, fntNet);
            renderText((float)(borderW - (1 - borderW)/1.25), (float)(i/3.0 - 1), QString().number(abs(i*30 - 90)), clrNetFont, fntNet);
        }
        for (int i = 1; i < 12; i++) {
            tmp = i/6.0 - 1.0;
            renderText(tmp, (float)(-1 + (1 - borderH)/0.5), QString().number(abs(i*30 - 180)), clrNetFont, fntNet);
            renderText(tmp, (float)(borderH + (1 - borderH)/1.25), QString().number(abs(i*30 - 180)), clrNetFont, fntNet);
        }
    glEndList();
}

void GLSatWidget::compileSatList() {
    float px, py, tmpx, tmpy, tper = 0;
    float trackBegin, trackEnd;
    uint8_t shadow_state, shadow_tmp;
    uint32_t clr;
    Satellite *sat = 0;
    
    if (satList.count() < 1) {
        glNewList(list_sat, GL_COMPILE);
        glEndList();
        return;
    }

    glNewList(list_sat, GL_COMPILE);
        for (int i = 0; i < satList.count(); i++) {
            sat = satList.at(i);
            shadow_state = 2;
            sat->model(m_time);
            if (sat->isVisibleZrv() || sat->isVisibleLines()) compileZRV(sat, sat->isVisibleZrv(), sat->isVisibleLines(), sat->colorZrv(), sat->colorLines());

            if (sat->isVisibleTrack()) {
                //		    	clr = sat->colorTrack();
                //				glColor4ubv((uint8_t *)&clr);
                glShadeModel(GL_SMOOTH);
                glEnable(GL_ALPHA_TEST);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glEnable(GL_BLEND);
                glEnable(GL_LINE_SMOOTH);
                glLineWidth(sat->linesWidth());

                glBegin(GL_LINE_STRIP);
                    tper = 120*M_PI/sat->meanMotion(); //move this line
                    trackBegin = sat->track()*(-0.5*tper + tper/180.0);
                    trackEnd = sat->track()*(0.5*tper + tper/180.0);
                    sat->model(trackBegin + m_time);
                    tmpx = sat->longitude()/M_PI;
                    tmpy = -2*sat->latitude()/M_PI;
                    for (float i = trackBegin; i < trackEnd; i += tper/180.0) {
                        sat->model(i + m_time);
                        if (sat->isVisibleTrackShadow()) {
                            shadow_tmp = shadow_state;
//                            shadow_state = testShadow(sat, sun);
                            //							if ((shadow_tmp == 0) && (shadow_state == 1)) {
                            //								glEnd();
                            //								glPushMatrix();
                            //								glTranslated(px, py, 0);
                            //								glCallList(list_shadow_in);
                            //								glPopMatrix();
                            //								glBegin(GL_LINE_STRIP);
                            //							}
                            //							if ((shadow_tmp == 1) && (shadow_state == 0)) {
                            //
                            //							}
                            if (shadow_state) {
                                clr = sat->colorTrack();
                                glColor4ubv((uint8_t *)&clr);
                            }
                            else {
                                clr = sat->colorTrackShadow();
                                glColor4ubv((uint8_t *)&clr);
                            }
                        }
                        else {
                            clr = sat->colorTrack();
                            glColor4ubv((uint8_t *)&clr);
                        }
                        px = sat->longitude()/M_PI;
                        py = -2*sat->latitude()/M_PI;
                        if (fabs(px - tmpx) > 1.75) {
                            if (px > tmpx) {
                                glVertex2f(-1, 0.5*(py + tmpy));
                                glEnd(); glBegin(GL_LINE_STRIP);
                                glVertex2f( 1, 0.5*(py + tmpy));
                            }
                            else {
                                glVertex2f( 1, 0.5*(py + tmpy));
                                glEnd(); glBegin(GL_LINE_STRIP);
                                glVertex2f(-1, 0.5*(py + tmpy));
                            }
                        }
                        glVertex2f(px, py);
                        tmpx = px;
                        tmpy = py;
                    }
                glEnd();

                glDisable(GL_BLEND);
                glDisable(GL_LINE_SMOOTH);
            }

            sat->model(m_time);
            px = sat->longitude()/M_PI;
            py = -2*sat->latitude()/M_PI;
            if (sat->satWObject != 0) sat->satWObject->exec(px, py);

            if (sat->isVisibleLabel()) renderText(px + (float)sat->nameX()/width(), py + (float)sat->nameY()/height(), sat->name(), sat->colorLabel(), sat->font());
        }

        sat = currentSat();
        if (sat == 0) sat = satList.first();
        sat->model(m_time);
        px = sat->longitude()/M_PI;
        py = -2*sat->latitude()/M_PI;
        sprite_current.exec(px, py);
    glEndList();
}

void GLSatWidget::compileLocList() {
    float px, py;
    Location *loc;

    if (locList.count() < 1) {
        glNewList(list_loc, GL_COMPILE);
        glEndList();
        return;
    }

    glNewList(list_loc, GL_COMPILE);
        for (int i = 0; i < locList.count(); i++) {
            loc = locList.at(i);
            compileZrl(loc);
            if (loc->isVisibleLabel()) {
                px = loc->longitude()/180.0;
                py = -loc->latitude()/90.0;
                renderText(px + (float)loc->nameX()/width(), py + (float)loc->nameY()/height(), loc->name(), loc->colorLabel(), loc->font());
            }
        }

        loc = currentLoc();
        if (loc == 0) loc = locList.first();
        px = loc->longitude()/180.0;
        py = -loc->latitude()/90.0;
        sprite_current.exec(px, py);
    glEndList();
}

void GLSatWidget::compileEventsList() {
    return;
    float px, py;
    Location *loc;
    Satellite *sat;
    int inZRV, statusZRV;
    double oldTime;
    QString msg;
    QDateTime tmpTime;

    if (locList.count() < 1) {
        glNewList(list_events, GL_COMPILE);
        glEndList();
    	return;
    }

    glNewList(list_events, GL_COMPILE);
        for (int i = 0; i < locList.count(); i++) {
            loc = locList.at(i);
            inZRV = 0;
            if (loc->isActiveZone()) {
                for (int j = 0; j < satList.count(); j++) {
                    sat = satList.at(j);
                    if (!sat->isAtctiveZone()) continue;
                    oldTime = m_time;
                    statusZRV = testIOZRV(sat, loc, &ioList, oldTime);
                    if (statusZRV > -1) inZRV++;
                    if (statusZRV == 1) {
                        tmpTime = QDateTime::fromTime_t((int)m_time);
                        msg = QString("%0|%1|%2|%3|%4")
                            .arg(sat->name())
                            .arg("in")
                            .arg(loc->name())
                            .arg(tmpTime.time().toString())
                            .arg("*");
                        emit statusZRVChanged(msg);
                    }
                    if (statusZRV == 2) {
                        tmpTime = QDateTime::fromTime_t((int)m_time);
                        msg = QString("%0|%1|%2|%3|%4")
                            .arg(sat->name())
                            .arg("out")
                            .arg(loc->name())
                            .arg(tmpTime.time().toString())
                            .arg(m_time - oldTime);
                        emit statusZRVChanged(msg);
                    }
                }
            }

            px = loc->longitude()/180.0;
            py = -loc->latitude()/90.0;
            loc->satWObject->exec(px, py);

            if (inZRV > 0) sprite_active.exec(px, py);
        }
    glEndList();
}

void GLSatWidget::compileSunList() {
    return;
    glNewList(list_sun, GL_COMPILE);
//        sun->model(m_time);
//        float px =  sun->longitude()/M_PI;
//        float py = -2*sun->latitude()/M_PI;
//
//        if (shwNight) {
//            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//            glEnable(GL_BLEND);
//            compileZRV(sun, false, false, 0, 0);
//            glColor4ubv((uint8_t *)&clrNight);
//            glZoneNight(sun->latitude());
//            if (true) {
//                glColor3f(0.4, 0.4, 0.4);
//                glZoneLines(sun->latitude());
//            }
//                glDisable(GL_BLEND);
//        }
//
//        if (shwSun) {
//        sun->satWObject->exec(px, py);
//        }
    glEndList();
}

void GLSatWidget::renderText(float x, float y, const QString& text, int color, const QFont &font) {
    QFontMetrics fontMetrics(font);
    QRect rect = fontMetrics.boundingRect(text);
    rect.setWidth(rect.width() + fontMetrics.averageCharWidth());
    QPixmap pixmap(rect.size());
    pixmap.fill(QColor(0, 0, 0, 0));
    QPainter painter(&pixmap);
    painter.setPen(color);
    painter.setFont(font);
    painter.drawText(-rect.left(), -rect.top(), text);

    QImage img = pixmap.toImage();
    img = img.mirrored(false, true);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glRasterPos2f(x, y);
    glDrawPixels(rect.width(), rect.height(), GL_RGBA, GL_UNSIGNED_BYTE, img.bits());
    glDisable(GL_BLEND);
}

void GLSatWidget::mouseMoveEvent(QMouseEvent *event) {
    float px, py;
    float w = this->width();
    float h = this->height();

    float x = event->x();
    float y = event->y();
    float kxl = w/360.0;
    float kyl = h/180.0;
    float kxs = 0.5*w/M_PI;
    float kys = h/M_PI;
    int resultLoc = -1;
    int resultSat = -1;
    Location *loc;
    Satellite *sat;

    for (int i = 0; i < locList.count(); i++) {
        loc = locList.at(i);
        px = kxl*(180.0 + loc->longitude())*m_zoom - 0.5*(m_zoom - 1 - m_dx)*w;
        py = kyl*( 90.0 - loc->latitude() )*m_zoom - 0.5*(m_zoom - 1 - m_dy)*h;
        if (fabs(px - x) < 7*m_zoom && fabs(py - y) < 7*m_zoom) {
            resultLoc = i;
            break;
        }
    }

    if ((event->buttons() == Qt::LeftButton) && (resultLoc != -1)) setIndexLoc(resultLoc);

    for (int i = 0; i < satList.count(); i++) {
        sat = satList.at(i);
        px = kxs*(    M_PI + sat->longitude())*m_zoom - 0.5*(m_zoom - 1 - m_dx)*w;
        py = kys*(0.5*M_PI - sat->latitude() )*m_zoom - 0.5*(m_zoom - 1 - m_dy)*h;
        if (fabs(px - x) < 7*m_zoom && fabs(py - y) < 7*m_zoom) {
            resultSat = i;
            break;
        }
    }

    if ((event->buttons() == Qt::LeftButton) && (resultSat != -1)) setIndexSat(resultSat);

    if (resultSat > -1 || resultLoc > -1) {
        setCursor(Qt::PointingHandCursor);
    }
    else setCursor(Qt::CrossCursor);

    if (event->buttons() == Qt::RightButton) {
        moveX(2*(event->x() - pointMoveMap.x())/w);
        moveY(2*(event->y() - pointMoveMap.y())/h);
        pointMoveMap = event->pos();
        paintGL();
    }
    event->accept();
}

void GLSatWidget::mousePressEvent(QMouseEvent *event) {
    if (event->buttons() == Qt::LeftButton) {
        mouseMoveEvent(event);
        refreshAll();
    }
    if (event->buttons() == Qt::RightButton) {
        pointMoveMap = event->pos();
    }
    event->accept();
}

void GLSatWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->buttons() == Qt::LeftButton) {
        int tmp = m_indexSat;
        m_indexSat = -1;
        this->mouseMoveEvent(event);
        if (m_indexSat != -1) emit this->doubleClickedSat();
        else {
            m_indexSat = tmp;
            tmp = m_indexLoc;
            m_indexLoc = -1;
            this->mouseMoveEvent(event);
            if (m_indexLoc != -1) emit this->doubleClickedLoc();
            else m_indexLoc = tmp;
        }
    }
    event->accept();
}

void GLSatWidget::wheelEvent(QWheelEvent *event) {
    if (event->buttons() == Qt::RightButton) {
        zoom(0.001*event->delta());
        paintGL();
    }
    event->accept();
}

float GLSatWidget::zoom(float value) {
    ui.spinZoom->blockSignals(true);
    ui.spinX->blockSignals(true);
    ui.spinY->blockSignals(true);
    GLSatAbstractWidget::zoom(value);
    ui.spinZoom->setValue(m_zoom);
    ui.spinX->setValue(m_dx);
    ui.spinY->setValue(m_dy);
    ui.spinZoom->blockSignals(false);
    ui.spinX->blockSignals(false);
    ui.spinY->blockSignals(false);
    return m_zoom;
}

float GLSatWidget::moveX(float value) {
    ui.spinX->blockSignals(true);
    GLSatAbstractWidget::moveX(value);
    ui.spinX->setValue(m_dx);
    ui.spinX->blockSignals(false);
    return m_dx;
}

float GLSatWidget::moveY(float value) {
    ui.spinY->blockSignals(true);
    GLSatAbstractWidget::moveY(value);
    ui.spinY->setValue(m_dy);
    ui.spinY->blockSignals(false);
    return m_dy;
}

void GLSatWidget::changeZoom(double value) {
    zoom(value - m_zoom);
}

void GLSatWidget::changeX(double value) {
    moveX(value - m_dx);
}

void GLSatWidget::changeY(double value) {
    moveY(value - m_dy);
}
