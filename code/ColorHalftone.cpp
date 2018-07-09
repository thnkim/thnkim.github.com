//
// Made by Sang Il Park (sipark@sejong.ac.kr)
//
// Simply build by (at Terminal)
//     c++ -o ColorHalftone ColorHalftone.cpp `pkg-config --cflags --libs opencv`
// And then
//     ./ColorHalftone -h
// to see how-to-use.
//

#include <opencv2/opencv.hpp>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

IplImage * src;
IplImage * dst;
IplImage * sprite[4];

#define _HALFTONE_CMY_		0
#define _HALFTONE_CMYK_		1

int halftoneMode = _HALFTONE_CMYK_;

std::string input_path = "c:\\flower.jpg";
std::string output_path = "";

float scale = 5.0f;
int dot_maxSize = 80;
float dot_spacing_scale = sqrt(2);
//float dot_spacing_scale = 1;
float key_contribution = 1.0f;
float gridAngle = 90;
int spriteDownscale = 8;									// usually dot_maxSize/10 is a good value

//float angle[4] = { 90, 45, 165, 105 };
float angle[4] = { 0, 15, 30, 46 };
float * areaBright = new float[dot_maxSize];
int maxChannel;

float getBri(CvScalar c)
{
	return (c.val[0] + c.val[1] + c.val[2]) / 3.0f;
}

int getRadi(float intensity, float * areaBri, int l)
{
	for (int i = 0; i < l; i++)
	{
		if (intensity > areaBri[i])
			return i;
	}
	return l - 1;
}

IplImage * downscaleImage(IplImage * in, float s)
{
	CvSize inSize = cvGetSize(in);
	CvSize outSize = cvSize(inSize.width / s, inSize.height / s);
	IplImage * out = cvCreateImage(outSize, 8, 3);
	IplImage * inBlur = cvCreateImage(inSize, 8, 3);
	cvSmooth(in, inBlur, CV_BLUR, s*2 - 1);
	for (int y = 0; y<outSize.height; y++)
		for (int x = 0; x < outSize.width; x++)
		{
			int x2 = x * s;	if (x2<0 || x2>inSize.width - 1) continue;
			int y2 = y * s; if (y2<0 || y2>inSize.height - 1) continue;
			cvSet2D(out, y, x, cvGet2D(inBlur, y2, x2));
		}
	cvReleaseImage(&inBlur);
	return out;
}

IplImage * createSprite(int s, float * brightness, int channel = -1)
{
	int w = s * 2 * s;								
	int h = s * 2;
	IplImage * img = cvCreateImage(cvSize(w, h), 8, 3);
	cvSet(img, cvScalar(0, 0, 0));
	float areaOfRect = 2 * s * s;					// 4*s*s for full size but 2*s*s for overlapping 
	for (int i = 0; i < s; i++)
	{
		int x = s + s * 2 * i;
		int y = s;
		CvScalar c = cvScalar(0, 0, 0);
		if (channel >= 0)
			c.val[channel] = 255;
		else
			c = cvScalar(255, 255, 255);
		cvCircle(img, cvPoint(x, y), i, c, -1);
		brightness[i] = 0;
		float ds = sqrt(2.0) / 2.0*s;
		float area = 0;
		for (int v = y - ds; v <= y + ds; v++)
			for (int u = x - ds; u <= x + ds; u++)
			{
				CvScalar f = cvGet2D(img, v, u);
				if (f.val[0] + f.val[1] + f.val[2] < 1)
					brightness[i] += 1;
				area += 1;
			}
		brightness[i] = brightness[i] / (area) * 255;
//		brightness[i] = ((areaOfRect - 3.141592*i*i) / (areaOfRect)) * 255;
//		printf("%d brightness % : %f \n", i, brightness[i]);
	}
	
	if (spriteDownscale == 1) return img;
	IplImage * out = downscaleImage(img, spriteDownscale);
	cvReleaseImage(&img);
	return out;
}

void subtractingSprite(IplImage * img, IplImage * spr, int px, int py, int r)
{
	int dx = dot_spacing_scale * dot_maxSize / spriteDownscale;
	int dy = dot_spacing_scale * dot_maxSize / spriteDownscale;
	int dw = 2 * dot_maxSize / spriteDownscale;
	int dh = 2 * dot_maxSize / spriteDownscale;

	for (int v = -dy / 2-1; v <= dy / 2+1; v++)
		for (int u = -dx / 2-1; u <= dx / 2+1; u++)
		{
			int x = px + u; if (x < 0 || x>img->width - 1) continue;
			int y = py + v; if (y<0 || y>img->height - 1) continue;
			int s = r * dw + u + dw/2;	if (s<0 || s>spr->width - 1) continue;
			int t = v + dh/2;			if (t<0 || t>spr->height - 1) continue;
			unsigned char * data;
			CvScalar f;
			data = (unsigned char *)img->imageData;
			f.val[0] = data[y*img->widthStep + x * 3 + 0];
			f.val[1] = data[y*img->widthStep + x * 3 + 1];
			f.val[2] = data[y*img->widthStep + x * 3 + 2];

			CvScalar g;
			data = (unsigned char *)spr->imageData;
			g.val[0] = data[t*spr->widthStep + s * 3 + 0];
			g.val[1] = data[t*spr->widthStep + s * 3 + 1];
			g.val[2] = data[t*spr->widthStep + s * 3 + 2];


			for (int k = 0; k < 3; k++)
				f.val[k] = MAX(f.val[k] - g.val[k], 0);

			data = (unsigned char *)img->imageData;
			data[y*img->widthStep + x * 3 + 0] = (unsigned char)f.val[0];
			data[y*img->widthStep + x * 3 + 1] = (unsigned char)f.val[1];
			data[y*img->widthStep + x * 3 + 2] = (unsigned char)f.val[2];
		}

}
CvScalar get2DBilinear(IplImage *img, float y, float x)
{
	unsigned char * data;
	data = (unsigned char *)img->imageData;

	int i1 = (int)x;
	int j1 = (int)y;

	float a = x - i1;
	float b = y - j1;

	int i2 = i1 + 1; if (i2>img->width - 1)  i2 = i1;
	int j2 = j1 + 1; if (j2>img->height - 1) j2 = j1;

	CvScalar f1;  //CvScalar f1 = cvGet2D(img, j1, i1);
	f1.val[0] = data[j1*img->widthStep + i1 * 3 + 0];
	f1.val[1] = data[j1*img->widthStep + i1 * 3 + 1];
	f1.val[2] = data[j1*img->widthStep + i1 * 3 + 2];
	CvScalar f2;  //CvScalar f2 = cvGet2D(img, j1, i2);
	f2.val[0] = data[j1*img->widthStep + i2 * 3 + 0];
	f2.val[1] = data[j1*img->widthStep + i2 * 3 + 1];
	f2.val[2] = data[j1*img->widthStep + i2 * 3 + 2];

	CvScalar f3;  //CvScalar f3 = cvGet2D(img, j2, i1);
	f3.val[0] = data[j2*img->widthStep + i1 * 3 + 0];
	f3.val[1] = data[j2*img->widthStep + i1 * 3 + 1];
	f3.val[2] = data[j2*img->widthStep + i1 * 3 + 2];

	CvScalar f4;  //CvScalar f4 = cvGet2D(img, j2, i2);
	f4.val[0] = data[j2*img->widthStep + i2 * 3 + 0];
	f4.val[1] = data[j2*img->widthStep + i2 * 3 + 1];
	f4.val[2] = data[j2*img->widthStep + i2 * 3 + 2];

	CvScalar f;

	for (int k = 0; k<3; k++)
		f.val[k] = (1 - a)*(1 - b)*f1.val[k]
		+ a*(1 - b)*f2.val[k]
		+ (1 - a)*b *f3.val[k]
		+ a*b*f4.val[k];
	return f;
}


// ColorHalftone lena.jpg lena_half.jpg -m 1 -s 10 -a 50 60 70 80
static void show_usage(std::string name)
{
	std::cerr << "Usage: " << name << " <option(s)>\n"
		<< "Options:\n"
		<< "\t-h,--help\t\t\t\tShow this help message\n"
		<< "\t-i, --input INPUT_PATH\t\t\tSpecify the source image file path : MENDATORY OPTION\n"
		<< "\t-o,--output OUTPUT_PATH\tSpecify the destination image file path.\n"
											<< "\t\t\t\t\t\tDefault is <source path>_halftoned.<ext>\n"
		<< "\t-m,--mode MODE\t\t\t\tSpecify the rendering mode. Input value is either CMY or CMYK.\n"
											<< "\t\t\t\t\t\t(default: CMYK)\n"
		<< "\t-s,--scale SCALE_VALUE\t\t\tSpecify the scale up value. the bigger gives the finer patterns.\n"
											<< "\t\t\t\t\t\t(default: 20)\n"
//		<< "\t-p,--pointsize POINT_SIZE\t\t\tSpecify the point max size. the bigger gives the smoother image.\n"
//											<< "\t\t\t\t\t\t(default: 30)\n"
		<< "\t-a,--angle ANGLE1, ANGLE2, ANGLE3..\tSpecify the angle offset values. CMY requires 3 values, \n"
											<< "\t\t\t\t\t\tand CMYK requires 4 values. (default: 0, 15, 30, 45)\n"
		<< "\t-t,--tile TILE_ANGLE\t\t\tSpecify tile angle. (default: 90) \n"
		<< std::endl;
}

int getArgFromCommandLine(int argc, char * argv[])
{
	std::vector <std::string> sources;
	std::string destination;
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if ((arg == "-h") || (arg == "--help")) {
			show_usage(argv[0]);
			return 0;
		}
		else if ((arg == "-i") || (arg == "--input")) {
			if (i + 1 < argc) {				// Make sure we aren't at the end of argv!
				i++;
				input_path = argv[i];	
				std::cout << "input_path: " << input_path << std::endl;
			}
			else { // Uh-oh, there was no argument to the destination option.
				std::cerr << "--input option requires one argument." << std::endl;
				return 0;
			}
		}
		else if ((arg == "-o") || (arg == "--output")) {
			if (i + 1 < argc) {				// Make sure we aren't at the end of argv!
				i++;
				output_path = argv[i];	
				std::cout << "output_path: " << output_path << std::endl;
			}
			else { // Uh-oh, there was no argument to the destination option.
				std::cerr << "--output option requires one argument." << std::endl;
				return 0;
			}
		}

		else if ((arg == "-m") || (arg == "--mode")) {
			if (i + 1 < argc) {				// Make sure we aren't at the end of argv!
				i++;
				std::string mode = argv[i];	
				if (mode == "CMY" || mode == "cmy")
				{
					halftoneMode = _HALFTONE_CMY_;
					std::cout << "Halftone Mode: CMY" << std::endl;
				}
				else if (mode == "CMYK" || mode == "cmyk")
				{
					halftoneMode = _HALFTONE_CMYK_;
					std::cout << "Halftone Mode: CMYK" << std::endl;
				}
			}
			else { // Uh-oh, there was no argument to the destination option.
				std::cerr << "--mode option requires one argument." << std::endl;
				return 0;
			}
		}

		else if ((arg == "-s") || (arg == "--scale")) {
			if (i + 1 < argc) {				// Make sure we aren't at the end of argv!
				i++;
				std::string s = argv[i];
				scale = atoi(s.c_str());
				std::cout << "scale: " << scale << std::endl;
			}
			else { // Uh-oh, there was no argument to the destination option.
				std::cerr << "--scale option requires one argument." << std::endl;
				return 0;
			}
		}
/*
		else if ((arg == "-p") || (arg == "--pointsize")) {
			if (i + 1 < argc) {				// Make sure we aren't at the end of argv!
				i++;
				std::string p = argv[i];
				dot_maxSize = atoi(p.c_str());
				std::cout << "pointsize: " << dot_maxSize << std::endl;
			}
			else { // Uh-oh, there was no argument to the destination option.
				std::cerr << "--pointsize option requires one argument." << std::endl;
				return 0;
			}
		}
*/
		else if ((arg == "-a") || (arg == "--angle")) {
			if (i + 4 < argc) {				// Make sure we aren't at the end of argv!
				
				std::string a;
				i++; a = argv[i];	angle[0] = atoi(a.c_str());
				i++; a = argv[i];	angle[1] = atoi(a.c_str());
				i++; a = argv[i];	angle[2] = atoi(a.c_str());
				if (halftoneMode == _HALFTONE_CMYK_)
				{
					i++; a = argv[i];	angle[3] = atoi(a.c_str());
				}
				std::cout << "angle: " << angle[0] <<" " << angle[1] << " " << angle[2] << " " << angle[3] << " "<<std::endl;
			}
			else { // Uh-oh, there was no argument to the destination option.
				std::cerr << "--angle option requires three(or four) argument." << std::endl;
				return 0;
			}
		}
		else if ((arg == "-t") || (arg == "--tileangle")) {
			if (i + 1 < argc) {				// Make sure we aren't at the end of argv!
				i++;
				std::string t = argv[i];
				gridAngle = atoi(t.c_str());
				std::cout << "tileangle: " << gridAngle << std::endl;
			}
			else { // Uh-oh, there was no argument to the destination option.
				std::cerr << "--tileangle option requires one argument." << std::endl;
				return 0;
			}
		}

	}
}



int main(int argc, char * argv[])
{
	if (argc > 1)
	{
		if (argc < 2) {
			show_usage(argv[0]);
			return 1;
		}
		if (getArgFromCommandLine(argc, argv) == 0) return 1;
	}
	if (input_path.length() < 1)
	{
		printf("Specify Input Path using -i option. For more info:\n");
		show_usage(argv[0]);
		return 1;
	}


	src = cvLoadImage(input_path.c_str());
	if (src == NULL)
	{
		printf("File Not Found : %s\n", input_path.c_str());
		return 1;
	}
	if (output_path.length() < 1)
	{
		output_path = input_path.substr(0, input_path.length() - 4);
		output_path += "_halftoned"+ input_path.substr(input_path.length() - 4, 4);
	}

	CvSize size = cvGetSize(src);
	CvSize dstSize = cvSize(size.width*scale, size.height*scale);
	dst = cvCreateImage(dstSize, 8, 3);
	IplImage * blur = cvCreateImage(size, 8, 3);
	
	if (dot_maxSize / scale > 1)
		cvSmooth(src, blur, CV_BLUR, int(dot_maxSize/spriteDownscale/ scale) * 2 - 1);
	else
		cvCopy(src, blur);

	cvShowImage("src", src);

	if (halftoneMode == _HALFTONE_CMY_)
		maxChannel = 3;
	if (halftoneMode == _HALFTONE_CMYK_)
		maxChannel = 4;
	
	sprite[0] = createSprite(dot_maxSize, areaBright, 0);
	sprite[1] = createSprite(dot_maxSize, areaBright, 1);
	sprite[2] = createSprite(dot_maxSize, areaBright, 2);
	sprite[3] = createSprite(dot_maxSize, areaBright, -1);
	//	cvShowImage("sprite", sprite[0]);
	cvWaitKey(1);

	cvSet(dst, cvScalar(255, 255, 255));



	for (int channel = 0; channel < maxChannel; channel++)
	{
		float offset_x = -dstSize.width * 2;
		float offset_y = -dstSize.height * 2;

		float du = dot_maxSize * dot_spacing_scale / spriteDownscale;
		float dv = dot_maxSize * dot_spacing_scale / spriteDownscale;

		int kx = dstSize.width / du;
		int ky = dstSize.height / dv;

		float angle_offset = angle[channel];
		float rad_u = angle_offset * 3.141592 / 180.0f;
		float rad_v = rad_u + gridAngle*3.141592 / 180.0f;
		float dirUx = cos(rad_u); 
		float dirUy = sin(rad_u);
		if (dirUx < 0) { dirUx = -dirUx; dirUy = -dirUy; }
		
		float dirVx = cos(rad_v);
		float dirVy = sin(rad_v);
		if (dirVx < 0) { dirVx = -dirVx; dirVy = -dirVy; }

		float aspect = (float)dst->width / dst->height;
		int start_j = -ky;
		int end_j = ky*2;

		for (int j = -ky * 10; j < ky * 10; j++)
		//for (int j = start_j; j <=end_j; j++)
		{
			int start_i = -(dirVx*dv*j) / (dirUx*du)-2;
			int end_i = (dst->width - (dirVx*dv*j)) / (dirUx*du) + 2;
			if (start_i > end_i)
				std::swap(start_i, end_i);
			for (int i = -kx * 10; i < kx * 10; i++)
			//for(int i = start_i; i<=end_i; i++)
			{
				//				int x = offset_x + dirUx * du*i + dirVx * dv*j;
				//				int y = offset_y + dirUy * du*i + dirVy * dv*j;
				int x = dirUx * du*i + dirVx * dv*j;
				int y = dirUy * du*i + dirVy * dv*j;

				//x = x % size.width;
				//y = y % size.height;
				float inx = (x / scale);
				float iny = (y / scale);

				if (x < -2 * dot_maxSize / spriteDownscale || x > dst->width + 2 * dot_maxSize / spriteDownscale) continue;
				if (y < -2 * dot_maxSize / spriteDownscale || y > dst->height + 2 * dot_maxSize / spriteDownscale) continue;

				if (inx < 0) inx = 0;
				if (inx > size.width - 1) inx = size.width - 1;
				if (iny < 0) iny = 0;
				if (iny > size.height - 1) iny = size.height - 1;

				//if (inx<0 || inx>size.width - 1) continue;
				//if (iny<0 || iny>size.height - 1) continue;

				CvScalar c = get2DBilinear(blur, iny, inx);
				float value = 0;
				if (halftoneMode == _HALFTONE_CMY_)
					value = 255-c.val[channel];
				else if (halftoneMode == _HALFTONE_CMYK_)
				{
					float max = MAX(MAX(c.val[0], c.val[1]), c.val[2]);
					max *= key_contribution;
					float k = 255 - max;
					if (channel == 3)
						value = k;
					else
						value = (255.0f - c.val[channel] - k) / ((255 - k)/255.0);
					if (value > 255) value = 255;
					if (value < 0) value = 0;
				}
				int radi = getRadi(255-value, areaBright, dot_maxSize);
				subtractingSprite(dst, sprite[channel], x, y, radi);
			}
		}
	}
	IplImage * dst2 = downscaleImage(dst, scale);
	if (output_path.length() > 1)
		cvSaveImage(output_path.c_str(), dst2);

	cvShowImage("dst", dst2);
	std::cout << "Done!" << std::endl;
	cvWaitKey();

	cvReleaseImage(&src);
	cvReleaseImage(&dst2);
	cvReleaseImage(&dst);
	cvReleaseImage(&sprite[0]);
	cvReleaseImage(&sprite[1]);
	cvReleaseImage(&sprite[2]);
	cvReleaseImage(&sprite[3]);
	delete[] areaBright;	

	return 0;
}
