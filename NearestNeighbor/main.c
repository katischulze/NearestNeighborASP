#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

// Define external functions and their arguments
extern char* window(char* data, int x, int y, int width, int height, int ogWidth);
extern char* zoom(char* data, int width, int height, int factor);
extern char* windowSISD(char* data, int x, int y, int width, int height, int ogWidth);
extern char* zoomSISD(char* data, int width, int height, int factor);
// Improvement suggestion
extern char* zoomIS(char* data, int width, int height, int factor);

#define INPUT_ERROR(position) printf("Incorrect input. @%d\n", position); return -1;

// Debug output 
enum DEBUG_MODE {NONE=0, ALL, READ, WRITE, WINDOW, ZOOM, STEPS};
static int DEBUG = NONE;

static char* data;					// image data
static int ogWidth, ogHeight;		// original dimensions
static char* filenameInput = "lena.bmp";
static char* filenameOutput = "copy.bmp";
static int width = -1, height = -1;	// new dimensions (before scaling)
static int x = 0, y = 0;			// offsets for window cutout
static int factor = 3;				// scale factor
static int executionMode = 0;		// 0 == SIMD, 1 == SISD, 2 == C, 3 == Improvement suggestion (SIMD)


static int readBMP() {
	// Open the filestream
	FILE *file;
	file = fopen(filenameInput, "rb");
	// Stop if file couldn't be opened
	if (file == NULL)
	{
		printf("Couldn't read file.");
		return -1;
	}

	char header[54];

	// Read header
	fread(header, sizeof(char), 54, file);

	// Capture dimensions
	ogWidth = * (int*) &header[18];
	ogHeight = * (int*) &header[22];

	if(DEBUG == ALL || DEBUG == READ){
		printf("Input data dimensions: %d : %d\n", ogWidth, ogHeight);
	}
	
	int padding = 0;

	// Calculate padding
	while ((ogWidth * 3 + padding) % 4 != 0)
	{
		padding++;
	}

	// Allocate memory to store data data (non-padded)
	data = (char*) malloc(ogWidth * ogHeight * 3 * sizeof(char));
	// Stop if malloc failed
	if (data == NULL)
	{
		printf("Error: Malloc failed\n");
		return -2;
	}

	// Read actual data
	fread(data, 3, ogWidth * ogHeight, file);

	// Close stream
	fclose(file);
	return 0;
}

static int writeBMP() {
	
	width *= factor;
	height *= factor;
	
	// fileHeader and infoHeader according to Wikipedia
	char fileHeader[14] = {'B','M', 0,0,0,0, 0,0,0,0, 54,0,0,0};
	char infoHeader[40] = {40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,24,0};

	int sizeBytes = width * height * 3;
	int fileSize = sizeBytes + sizeof(fileHeader) + sizeof(infoHeader);
	
	// Write information to headers
	fileHeader[2] = (char)(fileSize);
	fileHeader[3] = (char)(fileSize >> 8);
	fileHeader[4] = (char)(fileSize >> 16);
	fileHeader[5] = (char)(fileSize >> 24);

	infoHeader[4] = (char)(width);
	infoHeader[5] = (char)(width >> 8);
	infoHeader[6] = (char)(width >> 16);
	infoHeader[7] = (char)(width >> 24);
	infoHeader[8] = (char)(height);
	infoHeader[9] = (char)(height >> 8);
	infoHeader[10] = (char)(height >> 16);
	infoHeader[11] = (char)(height >> 24);

	infoHeader[20] = (char)(sizeBytes);
	infoHeader[21] = (char)(sizeBytes >> 8);
	infoHeader[22] = (char)(sizeBytes >> 16);
	infoHeader[23] = (char)(sizeBytes >> 24);

	infoHeader[28] = 32;

	// Loading the actual file
	FILE* file = fopen(filenameOutput, "wb");

	// Write headers
	fwrite(fileHeader, 1, 14, file);
	fwrite(infoHeader, 1, 40, file);

	// To fill line in case width % 4 != 0
	char buffer[3] = {0, 0, 0};

	if (width % 4 == 0) {
		// Writing the data if width is multiple of 4 pixels
		if(DEBUG == ALL || DEBUG == WRITE) printf("Width is multiple of 4. No buffer needed");
		fwrite(data, width * height, 3, file);
	}
	else {
		// Padding necessary to get to multiple of 4 pixels
		int buffersize = 4 - (width * 3) % 4;

		if(DEBUG == ALL || DEBUG == WRITE) printf("Buffersize: %d\n", buffersize);

		// Loop over data vertically and write linewise
		for (int y = 0; y < height; y++) {
			if (DEBUG == ALL || DEBUG == WRITE) printf("Write Row: %d\n", y);
			
			// Write first line
			fwrite(data, 3, width, file);

			// Increment pointer to next line
			data += width * 3 * sizeof(char);
			
			// Fill with buffer
			fwrite(buffer, 1, buffersize, file);
		}
		if (DEBUG == ALL || DEBUG == WRITE) printf("All Rows Finished!\n");
	}

	if (DEBUG == ALL || DEBUG == WRITE) printf("Write Finished!\n");

	// Close filestream
	fclose(file);

	if (DEBUG == ALL || DEBUG == WRITE) printf("File Closed!\n");
	
	return 0;
}


static char* windowC(char* data, int x, int y, int width, int height, int ogWidth) {

	// Allocate memory to store data data (non-padded)
	int sizeBytes = width * height * 3;
	char* newData = (char*) malloc(sizeBytes * sizeof(char));
	
	// Stop if malloc failed
	if (newData == NULL)
	{
		printf("Error: Malloc failed\n");
		return data;
	}
	
	// Add the offset to the pointer right away
	data += ((x + ogWidth * y) * 3);
	// Pointer to read from
	char* currentOldData = data;
	// Pointer to write to
	char* currentNewData = newData;
	// Last pixel of the new data
	char* endNewData = newData + sizeBytes;
	
	// Loop over all pixels of new data
	for(; currentNewData < endNewData;){
		
		// Loop over the current line and copy all bytes to new data
		for (int loopX = 0; loopX < width; ++loopX){
			*(currentNewData++) = *(currentOldData++);
			*(currentNewData++) = *(currentOldData++);
			*(currentNewData++) = *(currentOldData++);
		}
		// set currentLine to the next line
		data += ogWidth * 3;
		currentOldData = data;
	}

	return newData;
}


static char* zoomC(char* data, int width, int height, int factor) {
	// Allocate memory to store data data (non-padded)
	char* newData = (char*) malloc(height * width * factor * factor * 3 * sizeof(char));
	
	// Stop if malloc failed
	if (newData == NULL)
	{
		printf("Error: Malloc failed\n");
		return data;
	}
	
	// Last pointer of old data
	char* endNewData = data + width * height * 3;
	// Increment between pixels in new data
	int increment = factor * 3;
	// Increment between lines in new data
	int lineIncrement = increment * width * (factor - 1);
	
	char* currentOldData = data;
	char* currentNewData = newData;
	
	for(int counter = 0; currentOldData < endNewData;){
		if(DEBUG == ALL || DEBUG == ZOOM)
			printf("Zoom copy loop: \tCounter: %d, \twindowWidth: %d, \tindex old: %d, \tindex new: %d\n",
				counter, width, currentOldData - data, currentNewData - newData);
				
		// Write pixel and increment currentOldData
		*(currentNewData) = *(currentOldData++);
		*(currentNewData+1) = *(currentOldData++);
		*(currentNewData+2) = *(currentOldData++);
		
		// increment currentNewData
		currentNewData += increment;
		++counter;
		
		// if counter exceeds width, jump to next line
		if(counter >= width){
			currentNewData += lineIncrement;
			counter = 0;
		}
	}

	// Fill loop
	width *= factor;
	height *= factor;
	for(int loopX = 0; loopX < width; loopX++){
		for (int loopY = 0; loopY < height; loopY++) {
		
			// Calculate coordinates of pixel to take color from
			// Coordinates used are offset by +1 / +1, in order to calculate according to Nearest Neighbor algorithm described in the assignment.
			int parentX = loopX + (factor / 2) - ((loopX + (factor / 2)) % factor);
			int parentY = loopY + (factor / 2) - ((loopY + (factor / 2)) % factor);
			
			// Edge handling
			if(parentX == width) parentX -= factor;
			if(parentY == height) parentY -= factor;

			// The index in the array for the current pixel
			// Index of current pixel
			int currentIndex = (loopX + loopY * width) * 3;
			// Index of pixel to take color from
			int parentIndex = (parentX + parentY * width) * 3;
			
			// Write 3 bytes. One per color channel
			newData[currentIndex] = newData[parentIndex];
			newData[currentIndex + 1] = newData[parentIndex + 1];
			newData[currentIndex + 2] = newData[parentIndex + 2];
			
		}
	}
	return newData;
}

int main(int argc, char** argv)
{
	// get command line arguments
	// use -help in commandline to get a description of every command
	for(int i = 1; i < argc; ++i){
		if(0 == strcmp(argv[i], "-in")){
			// Check if there is a parameter following
			if(i+1 >= argc){INPUT_ERROR(0);}
			
			filenameInput = argv[++i];
		}
		else if(0 == strcmp(argv[i], "-out")){
			// Check if there is a parameter following
			if(i+1 >= argc){INPUT_ERROR(1);}
			
			filenameOutput = argv[++i];
		}
		else if(0 == strcmp(argv[i], "-dimen")){
			// Check if there are two parameters following
			if(i+2 >= argc){INPUT_ERROR(2);}
			
			width = atoi(argv[++i]);
			height = atoi(argv[++i]);
		}
		else if(0 == strcmp(argv[i], "-offset")){
			// Check if there are two parameters following
			if(i+2 >= argc){INPUT_ERROR(3);}
			
			x = atoi(argv[++i]);
			y = atoi(argv[++i]);
		}
		else if(0 == strcmp(argv[i], "-scale")){
			// Check if there is a parameter following
			if(i+1 >= argc){INPUT_ERROR(4);}
			
			factor = atoi(argv[++i]);
		}
		else if(0 == strcmp(argv[i], "-debug")){
			// Default to ALL if no parameter is following
			if(++i >= argc){
				DEBUG = ALL;
			}else if(0 == strcmp(argv[i], "read")){
				DEBUG = READ;
			}else if(0 == strcmp(argv[i], "write")){
				DEBUG = WRITE;
			}else if(0 == strcmp(argv[i], "window")){
				DEBUG = WINDOW;
			}else if(0 == strcmp(argv[i], "zoom")){
				DEBUG = ZOOM;
			}else if(0 == strcmp(argv[i], "steps")){
				DEBUG = STEPS;
			}else if(0 == strcmp(argv[i], "all")){
				DEBUG = ALL;
			}else{
				// Default to ALL if following argument is none of the above
				DEBUG = ALL;
				// --i, because following argument is likely to be a different parameter
				--i;
			}
		}
		else if(0 == strcmp(argv[i], "-mode")){
			// Check if there is a parameter following
			if(i+1 >= argc){INPUT_ERROR(5);}
			
			executionMode = atoi(argv[++i]);
		}
		else if(0 == strcmp(argv[i], "-help")){
			printf("\nADDITIONAL PARAMETERS:\n\n");
			
			printf("-in FILENAME\n");
			printf("\tstring FILENAME: Name of the input data to be loaded.\n");
			printf("\t\t(No quotation marks needed.)\n");
			printf("\t\t(Default: lena.bmp)\n");
			printf("\t(Currently supported formats: BMP.)\n\n");
			
			printf("-out FILENAME\n");
			printf("\tstring FILENAME: Name of the output image to be written.\n");
			printf("\t\t(No quotation marks needed.)\n");
			printf("\t\t(Default: copy.bmp)\n");
			printf("\t(Currently supported formats: BMP.)\n\n");
			
			printf("-dimen WIDTH HEIGHT\n");
			printf("\tint WIDTH: Width of the cutout to be taken from input picture.\n");
			printf("\t\t(Default: -1. -1 will be set to width of input data.)\n");
			printf("\tint HEIGHT: Height of the cutout to be taken from input picture.\n");
			printf("\t\t(Default: -1. -1 will be set to height of input data.)\n");
			printf("\t(Dimensions have to be <= dimensions of input data.)\n\n");
			
			printf("-offset X Y\n");
			printf("\tint X: Horizontal offset of cutout. Default: 0\n");
			printf("\tint Y: Vertical offset of cutout. Default: 0\n");
			printf("\t(Cutout dimensions + Offset have to fit inside input data.)\n\n");
			
			printf("-scale SCALE\n");
			printf("\tint SCALE: Scale factor to be applied to width AND height of cutout.\n");
			printf("\t\t(Has to be >0)\n\n");
			
			printf("-mode MODE\n");
			printf("\tint MODE: Execution mode.\n");
			printf("\t\t0: ASM SIMD (Default)\n");
			printf("\t\t1: ASM SISD\n");
			printf("\t\t2: C\n");
			printf("\t\t3: Improvement Suggestion\n");
			printf("\t\t\t(A version we believe to be an improvement over the assignment's)\n\n");
			
			printf("-debug {MODE}\n");
			printf("\tstring MODE: Debug mode. Optional.\n");
			printf("\t\t(No quotation marks needed.)\n");
			printf("\t\tall: All debug modes simultaneously. (Default)\n");
			printf("\t\tread: Debug loading of input data.\n");
			printf("\t\twrite: Debug writing of output data.\n");
			printf("\t\twindow: Debug newData function. (cutout from input data)\n");
			printf("\t\tzoom: Debug newData function. (upscaling of cutout)\n");
			printf("\t\tsteps: Display execution steps.\n");
			printf("\t\t(Some debug modes may not give any output. This is for development only.)\n");
			return 0;
		}
		// If parameter was not recognized
		else{printf("Parameter: %s: ", argv[i]); INPUT_ERROR(6);}
	}
	
	if(DEBUG == ALL || DEBUG == STEPS) printf("Input handling finished\n");
	
	// Read bitmap and check for incorrect input
	if(0 != readBMP()) return -1;
	if(DEBUG == ALL || DEBUG == STEPS) printf("Read finished\n");
	
	// Default width and height are equal to input's
	if(height == -1) height = ogHeight;
	if(width == -1) width = ogWidth;
	
	// Make sure input was valid
	if(height > ogHeight || width > ogWidth){
		
		printf("Invalid input: Dimensions too large!\n");
		return -1;
		
	}else if((height + y) > ogHeight || (width + x) > ogWidth){
		
		printf("Invalid input: Offset or dimensions too large!\n");
		return -1;
		
	}else if(height < 0 ||
		width < 0 ||
		x < 0 ||
		y < 0 ||
		factor < 0){
			
		printf("Invalid input: Negative input!\n");
		return -1;
		
	}else if(height == 0 ||
		width == 0 ||
		factor == 0){
			
		printf("Invalid input: 0 is not valid!\n");
		return -1;
		
	}else if(executionMode > 3 || executionMode < 0){
			
		printf("Invalid input: Unknown executionMode!\n");
		return -1;
		
	}
	
	if(DEBUG == ALL || DEBUG == STEPS) printf("Input checks finished\n");
	
	// Declare variables for time measuring
	clock_t start, end;

	// WINDOW
	// Switch to executionMode defined by input parameter
	// Capture time taken by execution of function and print.
	if(DEBUG == ALL || DEBUG == WINDOW)
		printf("Calling newData with following parameters:\nOffset: %d : %d\nDimensions: %d : %d\nogWidth: %d\n", x, y, width, height, width);
	switch(executionMode){
		case 0:
			start = clock();
			data = window(data, x, y, width, height, ogWidth);
			end = clock();
			printf("Window: Time taken: SIMD-Version: %6.6f\n", ((double) (end - start)) / CLOCKS_PER_SEC);
			break;
		case 1:
			start = clock();
			data = windowSISD(data, x, y, width, height, ogWidth);
			end = clock();
			printf("Window: Time taken: SISD-Version: %6.6f\n", ((double) (end - start)) / CLOCKS_PER_SEC);
			break;
		case 2:
			start = clock();
			data = windowC(data, x, y, width, height, ogWidth);
			end = clock();
			printf("Window: Time taken: C-Version: %6.6f\n", ((double) (end - start)) / CLOCKS_PER_SEC);
			break;
		case 3:
			// Use the regular SIMD version, as only the zoom function is improved
			start = clock();
			data = window(data, x, y, width, height, ogWidth);
			end = clock();
			printf("Window: Time taken: SIMD-Version: %6.6f\n", ((double) (end - start)) / CLOCKS_PER_SEC);
			break;
	}
	
	if(DEBUG == ALL || DEBUG == STEPS) printf("Window finished\n");

	// This should not be executed if DEBUG == ALL, as that would not allow debugging of zoom in that mode
	if(DEBUG == WINDOW){
		// Write output data to disc
		writeBMP();
		return 0;
	}
	
	// ZOOM
	// Switch to executionMode defined by input parameter
	// Capture time taken by execution of function and print.
	switch(executionMode){
		case 0:
			start = clock();
			data = zoom(data, width, height, factor);
			end = clock();
			printf("Zoom: Time taken: SIMD-Version: %6.6f\n", ((double) (end - start)) / CLOCKS_PER_SEC);
			break;
		case 1:
			start = clock();
			data = zoomSISD(data, width, height, factor);
			end = clock();
			printf("Zoom: Time taken: SISD-Version: %6.6f\n", ((double) (end - start)) / CLOCKS_PER_SEC);
			break;
		case 2:
			start = clock();
			data = zoomC(data, width, height, factor);
			end = clock();
			printf("Zoom: Time taken: C-Version: %6.6f\n", ((double) (end - start)) / CLOCKS_PER_SEC);
			break;
		case 3:
			start = clock();
			data = zoomIS(data, width, height, factor);
			end = clock();
			printf("Zoom: Time taken: Improved SIMD-Version: %6.6f\n", ((double) (end - start)) / CLOCKS_PER_SEC);
			break;
	}
	
	if(DEBUG == ALL || DEBUG == STEPS) printf("Zoom finished\n");
	
	// Write output data to disc
	writeBMP();

	if(DEBUG == ALL || DEBUG == STEPS) printf("Write finished\n");
    return 0;
}


