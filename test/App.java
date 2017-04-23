
class App {

    public static void main(String[] args) throws Exception {
        System.out.println("Java app started and running...");
        StringBuilder sb = new StringBuilder();
        for (int i = 0; ; i++) {
            sb.append(i);
            if (0 == i % 100) {
                Thread.sleep(1);
            }
        }
    }
}
