#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <errno.h>
#include <stdint.h>
#define sizeOfColumn(Struct,Column) sizeof(((Struct*)0)->Column)

const uint32_t NAME_MAX_SIZE = 32;
const uint32_t PASSWD_MAX_SIZE = 255;

typedef enum  { SELECT, INSERT }SQLType; //CURD first :) //UPDATE, DELETE
typedef enum { 
    EXECUTE_SUCCESS,
    EXECUTE_FAILURE,
    EXECUTE_PARAMETER_ERROR, 
    EXECUTE_TABEL_FULL_ERROR,
    EXECUTE_UNSUPPORT_CMD,
    EXECUTE_TABLE_EMPTY
}ExecuteResult;
typedef struct {
    uint32_t id;
    char usrname[NAME_MAX_SIZE];
    char passwd[PASSWD_MAX_SIZE];
}Row;
typedef struct {
    SQLType type;
    Row row;
}Statement;
// define Input wrapper/stream
typedef struct  {
    char *buffer;
    size_t bufferLength;
    ssize_t inputLength;
}InputBuffer;

//calc the size
const uint32_t ID_SIZE = sizeOfColumn(Row,id);
const uint32_t USRNAME_SIZE = sizeOfColumn(Row,usrname);
const uint32_t PASSWD_SIZE = sizeOfColumn(Row,passwd);
const uint32_t ROW_SIZE = ID_SIZE + USRNAME_SIZE + PASSWD_SIZE;
//column's position
const uint32_t ID_OFFSET = 0;
const uint32_t USRNAME_OFFSET = ID_OFFSET + ID_SIZE;
const uint32_t PASSWD_OFFSET = USRNAME_OFFSET + USRNAME_SIZE;
//page & row size
const uint32_t PAGE_SIZE = 4096; //注意,这里取4k是考虑到与OS的page统一,不代表DB一定需要这样设置,具体大小需要根据使用场景来调整,参考之前page讲解
const uint32_t ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;
const uint32_t MAX_tPAGES = 10; //如果实际使用树去存储,那么这里的表最大page数就只会限制一次在内存里保存多少page,而不能限制db的大小上限了.
const uint32_t MAX_tROWS = MAX_tPAGES * ROWS_PER_PAGE;
typedef struct {
    void* pages[MAX_tPAGES]; //在内存中每个page地址不一定相邻,所以我们还需要确定如何在内存中读/写一行
    uint32_t rowNum;
}Table;

//How to read a row if it shouldn't cross the page?
void* rowPosition(Table *table, uint32_t rowNum){
    uint32_t pageNum = rowNum / ROWS_PER_PAGE;
    printf("rowNum1:%d\n",rowNum);
    void *page = table->pages[pageNum];
    printf("pageAddr1:%p\n",page);
    if (page == NULL) { //!page
        //malloc memory to page only when we try to access it?
        page = table->pages[pageNum] = malloc(PAGE_SIZE);
        printf("pageAddr2:%p\n",page);
    }
    uint32_t rowOffset = rowNum % ROWS_PER_PAGE; //得到是最后一个page的行数,比如有100行,每页14行 --> 100%14=2
    uint32_t byteOffset = rowOffset * ROW_SIZE; //算出最后N行的地址
   //TODO: printf("rowPosition:%p\n",page + byteOffset);
    return page + byteOffset; //返回n行的最后地址
}

void printRow(Row *row){printf("User:[%d, %s, %s]\n", row->id, row->usrname, row->passwd);}

//convert data into memory (use void* first)
void serializeRow(Row *source ,void *destination){
    //printf("%d ,%d, %d ", ID_OFFSET, USRNAME_OFFSET, PASSWD_OFFSET);
    printf("%p ,%p, %p ", &(source->id), &(source->usrname), &(source->passwd));
    memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
    memcpy(destination + USRNAME_OFFSET, &(source->usrname), USRNAME_SIZE);
    memcpy(destination + PASSWD_OFFSET, &(source->passwd), PASSWD_SIZE);
    printRow(source);
    printf("destinationAddr:%p", destination);
    printRow(destination);
}

//get data from memory(reverse)
void deserializeRow(void *source,Row *destination){
    printRow(source);
    memcpy(&(destination->id), source + ID_OFFSET, ID_SIZE);
    memcpy(&(destination->usrname), source + USRNAME_OFFSET, USRNAME_SIZE);
    memcpy(&(destination->passwd), source + PASSWD_OFFSET, PASSWD_SIZE);
    printf("deserializeRow ");
    printRow(destination);
}

ExecuteResult executeInsert(Statement *statement,Table *table){
    if (table->rowNum < MAX_tROWS){
        Row *insertRow = &(statement->row);
        printf("rowAddr1:%p\n",&(statement->row));
        serializeRow(insertRow, rowPosition(table,table->rowNum));
        table->rowNum ++;
        printf("rowNum:%d\n",table->rowNum);
        return EXECUTE_SUCCESS;
    }
    return EXECUTE_TABEL_FULL_ERROR;
}

ExecuteResult executeSelect(Statement *statement,Table *table){
    if(table->rowNum==0){
        return EXECUTE_TABLE_EMPTY;
    }
    Row row;
    for(uint32_t i = 0; i < table->rowNum; ++i ){
        deserializeRow(rowPosition(table,i), &row);
        printRow(&row);
    }
    return EXECUTE_SUCCESS;
}

void printFormat() { printf("db -> "); }

/*Only need when use windows...On Unix u need delete the getline() below.
* Refference POSIX-getline() & StackOverflow. 
* BTW ,add "#define _GUN_SOURCE" before "stdio.h" is useless on win(clang/gcc) 
*/
ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
    size_t pos;
    int c;

    if (lineptr == NULL || stream == NULL || n == NULL) {
        errno = EINVAL;
        return -1;
    }

    c = fgetc(stream);
    if (c == EOF) {
        return -1;
    }

    if (*lineptr == NULL) {
        *lineptr = malloc(128); //auto malloc space
        if (*lineptr == NULL) {
            return -1;
        }
        *n = 128;
    }

    pos = 0;
    while(c != EOF) {
        if (pos + 1 >= *n) {
            size_t new_size = *n + (*n >> 2);
            if (new_size < 128) {
                new_size = 128;
            }
            char *new_ptr = realloc(*lineptr, new_size);
            if (new_ptr == NULL) {
                return -1;
            }
            *n = new_size;
            *lineptr = new_ptr;
        }

        ((unsigned char *)(*lineptr))[pos ++] = c;
        if (c == '\n') { //end when meet '\n',but doesn't drop it.
            break;
        }
        c = fgetc(stream);
    }

    (*lineptr)[pos] = '\0';
    return pos;
}


void readInput(InputBuffer* inputBuffer){
    // Note: On windows,u have to impl it or use fgets() instead...(unsafe)
     ssize_t  bytesRead = getline(&(inputBuffer->buffer),&(inputBuffer->bufferLength),stdin);
     //printf("size = %llu, buffer = %s ,bufferLen = %lld, inputLen = %llu\n",bytesRead,inputBuffer->buffer,inputBuffer->bufferLength,inputBuffer->inputLength); //if '%zd' is supported.. use it instead.
    if (bytesRead <= 0) {
        printf("Invalid input\n");
        exit(EXIT_FAILURE);
    }
    //ignore '\n' in end of line  (or \r\n? Refference:https://segmentfault.com/a/1190000004367243)
    inputBuffer->inputLength = bytesRead - 1;
    inputBuffer->buffer[bytesRead - 1] = 0 ;
}

ExecuteResult doSysCMD(InputBuffer *inputBuffer) {
    if (strcmp(inputBuffer -> buffer,".q") == 0){ //support exit first~ ...这里用.q代替.exit
        exit(EXIT_SUCCESS);
    }else {
        return EXECUTE_FAILURE;
    }
}

//然后CURD里肯定是优先实现读&写,也就是select & insert
ExecuteResult prepareStatement(InputBuffer *inputBuffer, Statement *statement){
    //strncmp()相比strcmp()只比较前n个字符,但注意遇到'\0'会提前结束.  (why use strcmp() & why not use else-if?)
    if (strncmp(inputBuffer -> buffer, "select", 6) == 0){
        statement -> type = SELECT;
        return EXECUTE_SUCCESS;
    }
    if (strncmp(inputBuffer -> buffer, "insert", 6) == 0){
        statement -> type = INSERT;
        //先实现写入:读取一整行字符串数据, 然后存哪? 想想之前的逻辑,这就应该存到Statement里了.可以丰富它的结构了.(如下)
        int propsNum = sscanf(inputBuffer->buffer, "insert %d %s %s", &(statement->row.id), statement->row.usrname, statement->row.passwd);
        printf("%d,%s\n",propsNum,inputBuffer->buffer);
        if (propsNum != 3){
            return EXECUTE_PARAMETER_ERROR;
        }
        return EXECUTE_SUCCESS;
    }
    return EXECUTE_UNSUPPORT_CMD;
   }

//修改之前的执行方法入参和返回值,error code用起来
ExecuteResult executeStatement(Statement *statement, Table *table){
      switch (statement->type){
          case SELECT:
              return executeSelect(statement, table);
          case INSERT:
              return executeInsert(statement, table);
  }
  return EXECUTE_FAILURE;
}

//init method
InputBuffer* newInputBuffer() {
    InputBuffer *inputBuffer = malloc(sizeof(InputBuffer));
    inputBuffer->buffer = NULL;
    inputBuffer->bufferLength = 0;
    inputBuffer->inputLength = 0;
    return inputBuffer;
}
Table* initTable(){
    Table *table = malloc(sizeof(Table));
    table->rowNum = 0;
    return table;
}

/*V0.3版:
 *1.db-shell的入口是一个死循环的读取输入-->执行-->显示输出的过程
 *2.vm+sql-parser
 *3.in-momory backend
 */
int main ( int argc, char const *argv[]) {
    InputBuffer *inputBuffer = newInputBuffer();
    Table *table = initTable();
    printf("Sqlite starts...\n");
    while (1) {
        printFormat();
        readInput(inputBuffer);
        
        if (inputBuffer -> buffer[0] == '.') { //sqlite定义系统命令都以'.'开头...
            if(doSysCMD(inputBuffer) == EXECUTE_SUCCESS) {
                continue;
            }else {
                printf("Command '%s' is unsupported .. \n", inputBuffer->buffer);
            }
        }
        
        Statement statement;
        /*进入SQL编译器
         **这里你会发现各种输入会有不同的错误可能,再用之前的if-else后续就非常臃肿了,也不能只有0/1
         **应该在之前的枚举里多定义几个错误类型 ,然后改用switch-case去判断. (不过目前有很多枚举警告...后面处理)
        */
        switch (prepareStatement(inputBuffer,&statement)){
            case EXECUTE_SUCCESS:
                break;
            case EXECUTE_PARAMETER_ERROR:
                printf("Syntax error,plz check ur input again(params' type & amount)\n");
                continue;
            case EXECUTE_UNSUPPORT_CMD:
                printf("Keyword '%s' is unsupported .. \n", inputBuffer->buffer); 
                continue; 
        }
         //VM执行
        switch (executeStatement(&statement, table)) {
            case EXECUTE_SUCCESS:
                break;
            case EXECUTE_TABEL_FULL_ERROR:
                printf("Table is full, del first..\n");
                break;
            case EXECUTE_TABLE_EMPTY:
                printf("Table is empty, add data..\n");
                break;
        }
    }
}