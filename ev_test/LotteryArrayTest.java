public class LotteryArrayTest{
    
     public int sgx_hook(){
          final int mm = 10;
          int[][] odds = new int[mm+1][];
          for(int n = 0 ; n <= mm ; n++){
            odds[n] = new int[n+1];
          }
          for(int n = 0; n <odds.length ; n++){
              for(int k = 0 ; k <odds[n].length ; k++){
                    int lo = 1;
                    for(int i =1; i <=k; i++){
                         lo = lo * (n-i+1) / i;
                    }
                    odds[n][k] = lo;
              }
          }
          return odds[6][5];
     }

     public static void main(String[] args){
          LotteryArrayTest a = new LotteryArrayTest();

          int m = a.sgx_hook();
          
          System.out.println(m);
     }
}
