// 가. 전연 변수 설정
int xAxis = 0;
int yAxis = 0;
int x_value = 0;
int A_motor_speed = 0;
int B_motor_speed = 0;
boolean y_dir = false;
boolean A_dir = false;  // A모터 회전 방향 변수, true: 정회전, false: 역회전을 의미
boolean B_dir = false;  // B모터 회전 방향 변수
unsigned long prev_millis = 0;  // 멀티태스킹 처리 변수(이전 시간)
unsigned long current_millis = 0;  // 멀티태스킹 처리 변수(현재 시간)

int A_motor_dir_pin = 4;  // Motor A(오른쪽) 방향핀(DIR1 or IN1)
int A_motor_speed_pin = 5;  // Motor A(오른쪽) 속도핀
int B_motor_dir_pin = 7;  // Motor B(왼쪽) 방향핀(DIR2 or IN3)
int B_motor_speed_pin = 6;  // Motor B(왼쪽) 속도핀

int middle_x = 504;  // x축 중앙 값(조이스틱이 중립상태일 때, x축 값 입력, 0 ~ 255까지 입력)
int middle_y = 513;  // y축 중앙 값(조이스틱이 중립상태일 때, y축 값 입력, 0 ~ 255까지 입력)
int deadzone_x = 100;  // x축 데드존(x축 입력을 설정한 값까지는 무시, 0 ~ 1023까지 입력)
int deadzone_y = 100;  // y축 데드존(y축 입력을 설정한 값까지는 무시, 0 ~ 1023까지 입력)

int speed_max = 255;  // 최대 속도(0 ~ 255까지 입력)
int A_speed_bal = 0;  // Motor A 속도 밸런스(숫자가 커질수록 Motor A가 느려짐, 0 ~ 255까지 입력)
int B_speed_bal = 0;  // Motor B 속도 밸런스(숫자가 커질수록 Motor B가 느려짐, 0 ~ 255까지 입력)
unsigned long interval = 500;  //멀티태스킹 시 작업 간의 시간 차이(밀리초 단위)

// 나. 아두이노 부팅 시 1회만 실행되는 코드
void setup() {
  Serial.begin(9600);                             
  pinMode(A_motor_dir_pin, OUTPUT);
  pinMode(A_motor_speed_pin, OUTPUT);
  pinMode(B_motor_dir_pin, OUTPUT);
  pinMode(B_motor_speed_pin, OUTPUT);
}

// 다. 아두이노 부팅 후 반복적으로 실행되는 코드
void loop() {
  current_millis = millis();
  xAxis = analogRead(A0);
  yAxis = analogRead(A1);

  // 1) 모니터링(주석 처리 해제 시 시리얼 모니터로 입력값, 출력값 확인 가능)  
  if(current_millis - prev_millis >= interval) {
    /** Serial.print("X: ");
    Serial.print(xAxis);
    Serial.print(" ,  ");
    Serial.print("Y: ");
    Serial.println(yAxis);
    Serial.print("A_motor_speed: ");
    Serial.println(A_motor_speed);
    Serial.print("B_motor_speed: ");
    Serial.println(B_motor_speed);
    Serial.print("A: ");
    Serial.print(A_dir);
    Serial.print(" ,  ");
    Serial.print("B: ");
    Serial.println(B_dir); **/
  }
  
  // 2) Y축 값 처리
  if (yAxis < middle_y - deadzone_y) {
    y_dir = false;
    A_motor_speed = map(yAxis, middle_y - deadzone_y, 0, 0, speed_max);
    B_motor_speed = map(yAxis, middle_y - deadzone_y, 0, 0, speed_max);
  }
  else if (yAxis > middle_y + deadzone_y) {
    y_dir = true;
    A_motor_speed = map(yAxis, middle_y + deadzone_y, 1023, 0, speed_max);
    B_motor_speed = map(yAxis, middle_y + deadzone_y, 1023, 0, speed_max);
  }

  // 3) 데드존(설정된 입력값까지는 무시)
  else {
    A_motor_speed = 0;
    B_motor_speed = 0;
  }
  
  // 4) X축 값 처리
  if (xAxis < middle_x - deadzone_x) {
    x_value = map(xAxis, middle_x - deadzone_x, 0, 0, speed_max);
    if (y_dir == true){
      A_dir = true;
      A_motor_speed = A_motor_speed + x_value;
      if (A_motor_speed > speed_max) {
        A_motor_speed = speed_max;
      }
      if (B_motor_speed >= x_value){
        B_dir = true;
        B_motor_speed = B_motor_speed - x_value;
      }
      else {
        B_dir = false;
        B_motor_speed = x_value - B_motor_speed;
      }
    }
    else {
      B_dir = false;
      B_motor_speed = B_motor_speed + x_value;
      if (B_motor_speed > speed_max) {
        B_motor_speed = speed_max;
      }
      if (A_motor_speed >= x_value){
        A_dir = false;
        A_motor_speed = A_motor_speed - x_value;
      }
      else {
        A_dir = true;
        A_motor_speed = x_value - A_motor_speed;
      }
    }
  }
  else if (xAxis > middle_x + deadzone_x) {
    x_value = map(xAxis, middle_x + deadzone_x, 1023, 0, speed_max);
    if (y_dir == true){
      B_dir = true;
      B_motor_speed = B_motor_speed + x_value;
      if (B_motor_speed > speed_max) {
        B_motor_speed = speed_max;
      }
      if (A_motor_speed >= x_value){
        A_dir = true;
        A_motor_speed = A_motor_speed - x_value;
      }
      else {
        A_dir = false;
        A_motor_speed = x_value - A_motor_speed;
      }
    }
    else {
      A_dir = false;
      A_motor_speed = A_motor_speed + x_value;
      if (A_motor_speed > speed_max) {
        A_motor_speed = speed_max;
      }
      if (B_motor_speed >= x_value){
        B_dir = false;
        B_motor_speed = B_motor_speed - x_value;
      }
      else {
        B_dir = true;
        B_motor_speed = x_value - B_motor_speed;
      }
    }
  }
  else {
    A_dir = y_dir;
    B_dir = y_dir;
  }
 
  // 5) 좌우 속도 밸런스
  A_motor_speed = A_motor_speed - A_speed_bal;
  B_motor_speed = B_motor_speed - B_speed_bal;
  if (A_motor_speed < 0) {
    A_motor_speed = 0;
  }
  if (B_motor_speed < 0) {
    B_motor_speed = 0;
  }

  // 6) 최종 모터 제어
  digitalWrite(A_motor_dir_pin, A_dir);  // Motor A 방향 제어
  digitalWrite(B_motor_dir_pin, B_dir);  // Motor B 방향 제어
  analogWrite(A_motor_speed_pin, A_motor_speed);  // Motor A 속도조절
  analogWrite(B_motor_speed_pin, B_motor_speed);  // Motor B 속도조절

  // 7) 멀티태스킹 제어
  if (current_millis - prev_millis > interval) {
    prev_millis = current_millis;  // 현재 시간을 이전 시간에 저장
  }
}