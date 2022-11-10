#include <mpi.h>
#include <iostream>
#include <string>
#include <fstream>
#include <stdio.h>
#include <cstring>
#include <cstdlib>
#include <math.h>
#include "bmp.h"

using namespace std;

//定義平滑運算的次數
#define NSmooth 1000

/*********************************************************/
/*變數宣告：                                             */
/*  bmpHeader    ： BMP檔的標頭                          */
/*  bmpInfo      ： BMP檔的資訊                          */
/*  **BMPSaveData： 儲存要被寫入的像素資料               */
/*  **BMPData    ： 暫時儲存要被寫入的像素資料           */
/*********************************************************/
BMPHEADER bmpHeader;
BMPINFO bmpInfo;
RGBTRIPLE **BMPSaveData = NULL;
RGBTRIPLE **BMPData = NULL;

/*********************************************************/
/*函數宣告：                                             */
/*  readBMP    ： 讀取圖檔，並把像素資料儲存在BMPSaveData*/
/*  saveBMP    ： 寫入圖檔，並把像素資料BMPSaveData寫入  */
/*  swap       ： 交換二個指標                           */
/*  **alloc_memory： 動態分配一個Y * X矩陣               */
/*********************************************************/
int readBMP( char *fileName);        //read file
int saveBMP( char *fileName);        //save file
void swap(RGBTRIPLE *a, RGBTRIPLE *b);
RGBTRIPLE **alloc_memory( int Y, int X );        //allocate memory

int main(int argc,char *argv[])
{
/*********************************************************/
/*變數宣告：                                             */
/*  *infileName  ： 讀取檔名                             */
/*  *outfileName ： 寫入檔名                             */
/*  startwtime   ： 記錄開始時間                         */
/*  endwtime     ： 記錄結束時間                         */
/*********************************************************/
	char *infileName = "input.bmp";
        char *outfileName = "output2.bmp";
	double startwtime = 0.0, endwtime=0;

        int id, numprocs;
        int totalHeight, totalWidth;
	MPI_Init(&argc,&argv);
        MPI_Comm_size(MPI_COMM_WORLD,&numprocs);
        MPI_Comm_rank(MPI_COMM_WORLD,&id);

        //Deriving a new MPI type
        MPI_Datatype MPI_RGBTRIPLE;
        int elementsLen[3] = {1,1,1};
        MPI_Aint displacements[3] = {0,1,2};
        MPI_Datatype typeList[3] = {MPI_UNSIGNED_CHAR,MPI_UNSIGNED_CHAR,MPI_UNSIGNED_CHAR};
        MPI_Type_struct(3, elementsLen, displacements, typeList, &MPI_RGBTRIPLE);
        MPI_Type_commit(&MPI_RGBTRIPLE);


	//讀取檔案:只有root!
        if(id==0){
            if ( readBMP( infileName) ){
                cout << "Read file successfully!!" << endl;
                totalHeight = bmpInfo.biHeight;
                totalWidth  = bmpInfo.biWidth;
            }else{
                cout << "Read file fails!!" << endl;
                return 1;
            }
        }
        MPI_Bcast(&totalHeight, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&totalWidth, 1, MPI_INT, 0, MPI_COMM_WORLD);

	//動態分配記憶體給暫存空間
        BMPData = alloc_memory(totalHeight, totalWidth);
        if(id!=0)
            BMPSaveData = alloc_memory(totalHeight, totalWidth);


        //將圖片區塊分給節點BMPData
        int localRecv_Size = (floor((double)totalHeight*(id+1) /numprocs) - ceil((double)totalHeight*id/numprocs)+2) * totalWidth;
        int localRecv_Bias = ceil((double)totalHeight*id/numprocs) > 0? ceil((double)totalHeight*id/numprocs)-1:0;
        int scatter_Block[numprocs]   = {};
        int scatter_Disp[numprocs] = {};
        for(int i=0; i<numprocs; i++){
            scatter_Block[i] = (floor((double)totalHeight*(i+1) /numprocs) - ceil((double)totalHeight*i/numprocs)+2) * totalWidth;
            scatter_Disp[i] = (ceil((double)totalHeight*i/numprocs) > 0? ceil((double)totalHeight*i/numprocs)-1:0 ) * totalWidth;
        }
        MPI_Scatterv(&BMPSaveData[0][0],scatter_Block,scatter_Disp, MPI_RGBTRIPLE,&BMPData[localRecv_Bias][0], localRecv_Size, MPI_RGBTRIPLE, 0, MPI_COMM_WORLD);
        if(id!=0)
            swap(BMPSaveData,BMPData);

        //最後一個節點會使用到第一行的數值
        if(id==0){
            MPI_Send(&BMPSaveData[0][0], totalWidth, MPI_RGBTRIPLE,numprocs-1,0,MPI_COMM_WORLD);
        }else if(id==numprocs-1){
            MPI_Status status;
            MPI_Recv(&BMPSaveData[totalHeight-1][0], totalWidth, MPI_RGBTRIPLE, 0,0 ,MPI_COMM_WORLD, &status);
        }


        //記錄開始時間
        MPI_Barrier(MPI_COMM_WORLD);
	startwtime = MPI_Wtime();

        //進行多次的平滑運算
	for(int count = 0; count < NSmooth ; count ++){
	    //把像素資料與暫存指標做交換
	    swap(BMPSaveData,BMPData);
	    //區塊起始與結束位置
            int block_Start = ceil(totalHeight * id / numprocs);
            int block_End = floor(totalHeight * (id+1) / numprocs);
	    //區塊之間交換之前計算過的邊rows
            MPI_Send(&BMPData[block_End-1][0], totalWidth, MPI_RGBTRIPLE, id==(numprocs-1) ? 0 : id+1, 0, MPI_COMM_WORLD);
            MPI_Recv(&BMPData[id==0? totalHeight-1 : block_Start-1][0], totalWidth, MPI_RGBTRIPLE, id==0 ? numprocs-1 : id-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            MPI_Send(&BMPData[block_Start][0], totalWidth, MPI_RGBTRIPLE, id==0 ? (numprocs-1) : id-1, 1, MPI_COMM_WORLD);
            MPI_Recv(&BMPData[id==(numprocs-1)? 0 : block_End][0], totalWidth, MPI_RGBTRIPLE, id==(numprocs-1) ? 0:id+1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	    //進行平滑運算
	    for(int i = block_Start; i<block_End ; i++)
	        for(int j =0; j<totalWidth ; j++){
		    /*********************************************************/
		    /*設定上下左右像素的位置                                 */
		    /*********************************************************/
		    int Top = i>0 ? i-1 : totalHeight-1;
		    int Down = i<totalHeight-1 ? i+1 : 0;
		    int Left = j>0 ? j-1 : totalWidth-1;
		    int Right = j<totalWidth-1 ? j+1 : 0;
		    /*********************************************************/
		    /*與上下左右像素做平均，並四捨五入                       */
		    /*********************************************************/
		    BMPSaveData[i][j].rgbBlue =  (double) (BMPData[i][j].rgbBlue+BMPData[Top][j].rgbBlue+BMPData[Top][Left].rgbBlue+BMPData[Top][Right].rgbBlue+BMPData[Down][j].rgbBlue+BMPData[Down][Left].rgbBlue+BMPData[Down][Right].rgbBlue+BMPData[i][Left].rgbBlue+BMPData[i][Right].rgbBlue)/9+0.5;
		    BMPSaveData[i][j].rgbGreen =  (double) (BMPData[i][j].rgbGreen+BMPData[Top][j].rgbGreen+BMPData[Top][Left].rgbGreen+BMPData[Top][Right].rgbGreen+BMPData[Down][j].rgbGreen+BMPData[Down][Left].rgbGreen+BMPData[Down][Right].rgbGreen+BMPData[i][Left].rgbGreen+BMPData[i][Right].rgbGreen)/9+0.5;
		    BMPSaveData[i][j].rgbRed =  (double) (BMPData[i][j].rgbRed+BMPData[Top][j].rgbRed+BMPData[Top][Left].rgbRed+BMPData[Top][Right].rgbRed+BMPData[Down][j].rgbRed+BMPData[Down][Left].rgbRed+BMPData[Down][Right].rgbRed+BMPData[i][Left].rgbRed+BMPData[i][Right].rgbRed)/9+0.5;
		}

	}

        //把資料彙整到root的 BMPData
        int localSend_Size = (floor((double)totalHeight*(id+1) /numprocs) - ceil((double)totalHeight*id/numprocs) )*totalWidth;
        int localSend_Bias = ceil((double)totalHeight*id/numprocs) > 0? ceil((double)totalHeight*id/numprocs):0;
        int gather_Block[numprocs]   = {};
        int gather_Disp[numprocs] = {};
        for(int i=0; i<numprocs; i++){
            gather_Block[i] = (floor((double)totalHeight*(i+1) /numprocs) - ceil((double)totalHeight*i/numprocs) )*totalWidth;
            gather_Disp[i] = (ceil((double)totalHeight*i/numprocs) > 0? ceil((double)totalHeight*i/numprocs):0 )*totalWidth;
        }
        MPI_Gatherv(&BMPSaveData[localSend_Bias][0], localSend_Size , MPI_RGBTRIPLE, &BMPData[0][0], gather_Block, gather_Disp, MPI_RGBTRIPLE, 0, MPI_COMM_WORLD);
        if(id==0)
            swap(BMPSaveData,BMPData);

	//得到結束時間，並印出執行時間
        MPI_Barrier(MPI_COMM_WORLD);
        endwtime = MPI_Wtime();


 	//寫入檔案
        if(id==0){
      	    cout << "The execution time = "<< endwtime-startwtime <<endl ;
	    cout << endwtime-startwtime <<endl;
            if ( saveBMP( outfileName ) )
                cout << "Save file successfully!!" << endl;
            else
                cout << "Save file fails!!" << endl;
        }

	free(BMPData[0]);
 	free(BMPSaveData[0]);
 	free(BMPData);
 	free(BMPSaveData);

 	MPI_Type_free(&MPI_RGBTRIPLE);
        MPI_Finalize();

        return 0;
}

/*********************************************************/
/* 讀取圖檔                                              */
/*********************************************************/
int readBMP(char *fileName)
{
	//建立輸入檔案物件
        ifstream bmpFile( fileName, ios::in | ios::binary );

        //檔案無法開啟
        if ( !bmpFile ){
                cout << "It can't open file!!" << endl;
                return 0;
        }

        //讀取BMP圖檔的標頭資料
    	bmpFile.read( ( char* ) &bmpHeader, sizeof( BMPHEADER ) );

        //判決是否為BMP圖檔
        if( bmpHeader.bfType != 0x4d42 ){
                cout << "This file is not .BMP!!" << endl ;
                return 0;
        }

        //讀取BMP的資訊
        bmpFile.read( ( char* ) &bmpInfo, sizeof( BMPINFO ) );

        //判斷位元深度是否為24 bits
        if ( bmpInfo.biBitCount != 24 ){
                cout << "The file is not 24 bits!!" << endl;
                return 0;
        }

        //修正圖片的寬度為4的倍數
        while( bmpInfo.biWidth % 4 != 0 )
        	bmpInfo.biWidth++;

        //動態分配記憶體
        BMPSaveData = alloc_memory( bmpInfo.biHeight, bmpInfo.biWidth);

        //讀取像素資料
    	//for(int i = 0; i < bmpInfo.biHeight; i++)
        //	bmpFile.read( (char* )BMPSaveData[i], bmpInfo.biWidth*sizeof(RGBTRIPLE));
        bmpFile.read( (char* )BMPSaveData[0], bmpInfo.biWidth*sizeof(RGBTRIPLE)*bmpInfo.biHeight);

        //關閉檔案
        bmpFile.close();

        return 1;

}
/*********************************************************/
/* 儲存圖檔                                              */
/*********************************************************/
int saveBMP( char *fileName)
{
 	//判決是否為BMP圖檔
        if( bmpHeader.bfType != 0x4d42 ){
                cout << "This file is not .BMP!!" << endl ;
                return 0;
        }

 	//建立輸出檔案物件
        ofstream newFile( fileName,  ios:: out | ios::binary );

        //檔案無法建立
        if ( !newFile ){
                cout << "The File can't create!!" << endl;
                return 0;
        }

        //寫入BMP圖檔的標頭資料
        newFile.write( ( char* )&bmpHeader, sizeof( BMPHEADER ) );

	//寫入BMP的資訊
        newFile.write( ( char* )&bmpInfo, sizeof( BMPINFO ) );

        //寫入像素資料
        //for( int i = 0; i < bmpInfo.biHeight; i++ )
        //        newFile.write( ( char* )BMPSaveData[i], bmpInfo.biWidth*sizeof(RGBTRIPLE) );
        newFile.write( ( char* )BMPSaveData[0], bmpInfo.biWidth*sizeof(RGBTRIPLE)*bmpInfo.biHeight );

        //寫入檔案
        newFile.close();

        return 1;

}


/*********************************************************/
/* 分配記憶體：回傳為Y*X的矩陣                           */
/*********************************************************/
RGBTRIPLE **alloc_memory(int Y, int X )
{
	//建立長度為Y的指標陣列
        RGBTRIPLE **temp = new RGBTRIPLE *[ Y ];
        RGBTRIPLE *temp2 = new RGBTRIPLE [ Y * X ];
        memset( temp, 0, sizeof( RGBTRIPLE ) * Y);
        memset( temp2, 0, sizeof( RGBTRIPLE ) * Y * X );

	//對每個指標陣列裡的指標宣告一個長度為X的陣列
        for( int i = 0; i < Y; i++){
                temp[ i ] = &temp2[i*X];
        }

        return temp;

}
/*********************************************************/
/* 交換二個指標                                          */
/*********************************************************/
void swap(RGBTRIPLE *a, RGBTRIPLE *b)
{
	RGBTRIPLE *temp;
	temp = a;
	a = b;
	b = temp;
}
