
int main() {
    int fd;
    char *buf1, *buf2; 
    char comp_decomp_succeed;
    buf1 = (char *)malloc(4096);
    buf2 = (char *)malloc(4096);
    char letters[26] = {'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z'};
    char let;
            int size = 0;
    int len = 0;

    for (int i=0;i<100;i++) {
        comp_decomp_succeed = 1;
        srand((unsigned int)i);
        for (int j=0;j<4096;j++) {
            //srand((unsigned int)time(NULL));
            len = rand()%10;
            if((size+len)<4096){
                size += len;
                let = letters[rand()%26];
                for (int k=0;k<len;k++){
                    buf1[j+k] = let;
           }
        j = j+len-1;
        }
    }
    if ((fd = open("/dev/sbulla", O_RDWR)) < 0) {
