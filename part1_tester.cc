/* part1 tester.  
 * Test whether fs_client -> fs_server -> inode_layer behave correctly
 */

#include "fs_client.h"
#include <stdio.h>

#define FILE_NUM 50
#define LARGE_FILE_SIZE 512*64
int DISK_ACCESS_LATENCY = 0;

#define iprint(msg) \
    printf("[TEST_ERROR]: %s\n", msg);
fs_client *ec;
int total_score = 0;

int test_create_and_getattr()
{
    int i, rnum;
    fs_protocol::fsid_t id;
    fs_protocol::attr a;

    printf("========== begin test create and getattr ==========\n");

    srand((unsigned)time(NULL));

    for (i = 0; i < FILE_NUM; i++) {
        rnum = rand() % 10;
        memset(&a, 0, sizeof(a));
        if (rnum < 3) {
            ec->create(fs_protocol::T_DIR, id);
            if ((int)id == 0) {
                iprint("error creating dir\n");
                return 1;
            }
            if (ec->getattr(id, a) != fs_protocol::OK) {
                iprint("error getting attr, return not OK\n");
                return 2;
            }
            if (a.type != fs_protocol::T_DIR) {
                iprint("error getting attr, type is wrong");
                return 3;
            }
        } else {
            ec->create(fs_protocol::T_FILE, id);
            if ((int)id == 0) {
                iprint("error creating dir\n");
                return 1;
            }
            if (ec->getattr(id, a) != fs_protocol::OK) {
                iprint("error getting attr, return not OK\n");
                return 2;
            }
            if (a.type != fs_protocol::T_FILE) {
                iprint("error getting attr, type is wrong");
                return 3;
            }
        }
    } 
    total_score += 10; 
    printf("========== pass test create and getattr ==========\n");
    return 0;
}

int test_indirect()
{
    int i, rnum;
    char *temp1 = (char *)malloc(LARGE_FILE_SIZE);
    char *temp2 = (char *)malloc(LARGE_FILE_SIZE);
    fs_protocol::fsid_t id1, id2;
    printf("begin test indirect\n");
    srand((unsigned)time(NULL));
    
    ec->create(fs_protocol::T_FILE, id1);
    for (i = 0; i < LARGE_FILE_SIZE; i++) {
        rnum = rand() % 26;
        temp1[i] = 97 + rnum;
    }
    std::string buf(temp1);
    if (ec->put(id1, buf) != fs_protocol::OK) {
        printf("error put, return not OK\n");
        return 1;
    }
    ec->create(fs_protocol::T_FILE, id2);
    for (i = 0; i < LARGE_FILE_SIZE; i++) {
        rnum = rand() % 26;
        temp2[i] = 97 + rnum;
    }
    std::string buf2(temp2);
    if (ec->put(id2, buf2) != fs_protocol::OK) {
        printf("error put, return not OK\n");
        return 2;
    }
    std::string buf_2;
    if (ec->get(id1, buf_2) != fs_protocol::OK) {
        printf("error get, return not OK\n");
        return 3;
    }
    if (buf.compare(buf_2) != 0) {
        std::cout << "error get large file, not consistent with put large file : " << 
            buf << " <->\n " << buf_2 << "\n";
        return 4;
    }
    std::string buf2_2;
    if (ec->get(id2, buf2_2) != fs_protocol::OK) {
        printf("error get, return not OK\n");
        return 5;
    }
    if (buf2.compare(buf2_2) != 0) {
        std::cout << "error get large file 2, not consistent with put large file : " << 
            buf2 << " <-> " << buf2_2 << "\n";
        return 6;
    }

    total_score += 10;
    printf("end test indirect\n");
    return 0;
}

int test_put_and_get()
{
    int i, rnum;
    fs_protocol::fsid_t id;
    fs_protocol::attr a;
    int contents[FILE_NUM];
    char *temp = (char *)malloc(10);

    printf("========== begin test put and get ==========\n");
    srand((unsigned)time(NULL));
    for (i = 0; i < FILE_NUM; i++) {
        memset(&a, 0, sizeof(a));
        id = (fs_protocol::fsid_t)(i+2);
        if (ec->getattr(id, a) != fs_protocol::OK) {
            iprint("error getting attr, return not OK\n");
            return 1;
        }
        if (a.type == fs_protocol::T_FILE) {
            rnum = rand() % 10000;
            memset(temp, 0, 10);
            sprintf(temp, "%d", rnum);
            std::string buf(temp);
            if (ec->put(id, buf) != fs_protocol::OK) {
                iprint("error put, return not OK\n");
                return 2;
            }
            contents[i] = rnum;
        }
    }
    for (i = 0; i < FILE_NUM; i++) {
        memset(&a, 0, sizeof(a));
        id = (fs_protocol::fsid_t)(i+2);
        if (ec->getattr(id, a) != fs_protocol::OK) {
            iprint("error getting attr, return not OK\n");
            return 3;
        }
        if (a.type == fs_protocol::T_FILE) {
            std::string buf;
            if (ec->get(id, buf) != fs_protocol::OK) {
                iprint("error get, return not OK\n");
                return 4;
            }
            memset(temp, 0, 10);
            sprintf(temp, "%d", contents[i]);
            std::string buf2(temp);
            if (buf.compare(buf2) != 0) {
                std::cout << "[TEST_ERROR] : error get, not consistent with put " << 
                    buf << " <-> " << buf2 << "\n\n";
                return 5;
            }
        }
    } 

    total_score += 10;
    test_indirect(); 
    printf("========== pass test put and get ==========\n");
    return 0;
}

int test_remove()
{
    int i;
    fs_protocol::fsid_t id;
    fs_protocol::attr a;
    
    printf("========== begin test remove ==========\n");
    for (i = 0; i < FILE_NUM; i++) {
        memset(&a, 0, sizeof(a));
        id = (fs_protocol::fsid_t)(i+2);
        if (ec->remove(id) != fs_protocol::OK) {
            iprint("error removing, return not OK\n");
            return 1;
        }
        ec->getattr(id, a);
        if (a.type != 0) {
            iprint("error removing, type is still positive\n");
            return 2;
        }
    }
    total_score += 10; 
    printf("========== pass test remove ==========\n");
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 1) {
        printf("Usage: ./part1_tester\n");
        return 1;
    }
  
    ec = new fs_client();

    if (test_create_and_getattr() != 0)
        goto test_finish;
    if (test_put_and_get() != 0)
        goto test_finish;
    if (test_remove() != 0)
        goto test_finish;

test_finish:
    printf("---------------------------------\n");
    printf("Part1 score is : %d/40\n", total_score);
    return 0;
}
