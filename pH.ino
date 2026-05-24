#define SensorPin A0            //pH meter Analog output to Arduino Analog Input 0
#define Offset 0.00             //deviation compensate
#define LED 13
#define samplingInterval 20     // pH 센서 샘플링 간격 (20ms)
#define printInterval 800       // pH 값 시리얼 출력 간격 (800ms)
#define ArrayLenth  40          // Times of collection for averaging
int pHArray[ArrayLenth];        // Store the average value of the sensor feedback
int pHArrayIndex=0;

// 펌프 제어 핀 정의
int acidPumpA = 10;   // 산성 용액 펌프 정방향 (AA -> acidPumpA)
int acidPumpB = 6;    // 산성 용액 펌프 역방향 (AB -> acidPumpB)
int basePumpA = 9;    // 염기성 용액 펌프 정방향 (BA -> basePumpA)
int basePumpB = 5;    // 염기성 용액 펌프 역방향 (BB -> basePumpB)

// 펌프 작동 타이머 변수
unsigned long acidPumpStartTime = 0; // 산성 펌프 시작 시간
unsigned long basePumpStartTime = 0; // 염기성 펌프 시작 시간

// 펌프 작동 시간 (밀리초)
const unsigned long PUMP_RUN_DURATION = 800; // 펌프 작동 시간 (200ms)
const unsigned long PUMP_STOP_DURATION_ACID = 5000; // 산성 펌프 정지 후 대기 시간 (5초)
const unsigned long PUMP_STOP_DURATION_BASE = 10000; // 염기성 펌프 정지 후 대기 시간 (10초)

// 펌프 상태 변수
bool isAcidPumpRunning = false;
bool isBasePumpRunning = false;
unsigned long acidPumpStopResumeTime = 0; // 산성 펌프 재개 가능 시간
unsigned long basePumpStopResumeTime = 0; // 염기성 펌프 재개 가능 시간

double avergearray(int* arr, int number){
  int i;
  int max,min;
  double avg;
  long amount=0;
  if(number<=0){
    Serial.println("Error number for the array to avraging!/n");
    return 0;
  }
  if(number<5){   //less than 5, calculated directly statistics
    for(i=0;i<number;i++){
      amount+=arr[i];
    }
    avg = amount/number;
    return avg;
  }else{
    if(arr[0]<arr[1]){
      min = arr[0];max=arr[1];
    }
    else{
      min=arr[1];max=arr[0];
    }
    for(i=2;i<number;i++){
      if(arr[i]<min){
        amount+=min;         //arr<min
        min=arr[i];
      }else {
        if(arr[i]>max){
          amount+=max;     //arr>max
          max=arr[i];
        }else{
          amount+=arr[i]; //min<=arr<=max
        }
      }//if
    }//for
    avg = (double)amount/(number-2);
  }//if
  return avg;
}

void setup(void)
{
  pinMode(LED,OUTPUT);
  Serial.begin(9600);
  Serial.println("pH meter experiment!");    //Test the serial monitor

  pinMode(acidPumpA, OUTPUT);   // 산성 펌프 핀 설정
  pinMode(acidPumpB, OUTPUT);
  pinMode(basePumpA, OUTPUT);   // 염기성 펌프 핀 설정
  pinMode(basePumpB, OUTPUT);

  // 초기 상태: 모든 펌프 정지
  digitalWrite(acidPumpA, LOW);
  digitalWrite(acidPumpB, LOW);
  digitalWrite(basePumpA, LOW);
  digitalWrite(basePumpB, LOW);
}

void loop(void)
{
  static unsigned long samplingTime = millis();
  static unsigned long printTime = millis();
  static float pHValue,voltage;
  unsigned long currentMillis = millis(); // 현재 시간은 loop() 시작 시 한 번만 가져옴
  // pH 센서 값 샘플링 및 계산 (기존 코드 유지)
  if(currentMillis - samplingTime > samplingInterval)
  {
      pHArray[pHArrayIndex++]=analogRead(SensorPin);
      if(pHArrayIndex==ArrayLenth)pHArrayIndex=0;
      voltage = avergearray(pHArray, ArrayLenth)*5.0/1024;
      pHValue = 3.5*voltage+Offset; // 기존 코드에서 Offset이 -로 되어있는데 +가 더 일반적입니다. 확인해주세요.
      samplingTime=currentMillis; // 현재 시간을 다음 샘플링 기준으로 업데이트
  }

  // pH 값 시리얼 출력 (기존 코드 유지)
  if(currentMillis - printTime > printInterval)   //Every 800 milliseconds, print a numerical, convert the state of the LED indicator
  {
    Serial.print("Voltage:");
    Serial.print(voltage,2);
    Serial.print("      pH value: ");
    Serial.println(pHValue,2);
    digitalWrite(LED,digitalRead(LED)^1);
    printTime=currentMillis; // 현재 시간을 다음 출력 기준으로 업데이트
  }

  // 1. 산성 용액 펌프 제어
  if (pHValue > 7.5 && !isAcidPumpRunning && currentMillis >= acidPumpStopResumeTime) {
    // pH가 8.0보다 높고, 펌프가 현재 작동 중이 아니며, 이전에 펌프가 멈췄다면 재개 가능한 시간인지 확인
    digitalWrite(acidPumpA, HIGH);
    digitalWrite(acidPumpB, LOW);
    Serial.println("산성 용액 펌프 작동 시작!");
    isAcidPumpRunning = true;
    acidPumpStartTime = currentMillis; // 펌프 작동 시작 시간 기록
  }

  // 산성 펌프가 작동 중이고, PUMP_RUN_DURATION만큼 시간이 지났다면 멈춤
  if (isAcidPumpRunning && (currentMillis - acidPumpStartTime >= PUMP_RUN_DURATION)) {
    digitalWrite(acidPumpA, LOW);
    digitalWrite(acidPumpB, LOW);
    Serial.println("산성 용액 펌프 작동 멈춤.");
    isAcidPumpRunning = false;
    // 다음 산성 펌프 작동은 PUMP_STOP_DURATION_ACID 시간 후에 가능하도록 설정
    acidPumpStopResumeTime = currentMillis + PUMP_STOP_DURATION_ACID;
  }

  // 2. 염기성 용액 펌프 제어
  if (pHValue < 7 && !isBasePumpRunning && currentMillis >= basePumpStopResumeTime) {
    // pH가 6.0보다 낮고, 펌프가 현재 작동 중이 아니며, 이전에 펌프가 멈췄다면 재개 가능한 시간인지 확인
    digitalWrite(basePumpA, HIGH);
    digitalWrite(basePumpB, LOW);
    Serial.println("염기성 용액 펌프 작동 시작!");
    isBasePumpRunning = true;
    basePumpStartTime = currentMillis; // 펌프 작동 시작 시간 기록
  }

  // 염기성 펌프가 작동 중이고, PUMP_RUN_DURATION만큼 시간이 지났다면 멈춤
  if (isBasePumpRunning && (currentMillis - basePumpStartTime >= PUMP_RUN_DURATION)) {
    digitalWrite(basePumpA, LOW);
    digitalWrite(basePumpB, LOW);
    Serial.println("염기성 용액 펌프 작동 멈춤.");
    isBasePumpRunning = false;
    // 다음 염기성 펌프 작동은 PUMP_STOP_DURATION_BASE 시간 후에 가능하도록 설정
    basePumpStopResumeTime = currentMillis + PUMP_STOP_DURATION_BASE;
  }
}