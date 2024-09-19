## 프로토콜 플로우 및 패킷 구조

![스크린샷 2024-09-19 오후 12 03 32](https://github.com/user-attachments/assets/bfa4465e-6463-4ba5-a7c7-e4836730fa6b)

## 멀티 쓰레드 사용

본 프로젝트에서 Working Server는 5개의 PoW 연산 쓰레드와 1개의 Stop 리스너 쓰레드를 만들어 병렬처리를 한다.
pow_worker함수는 NUM_THREADS x RANGE_PER_THREAD의 총 합계를 NUM_THREADS 수의 쓰레드가 RANGE_PER_THREAD 단위로 분할하여 연산을 수행하 도록 구성된다. 현재 설정에 따르면, 10만 개의 숫자를 5개의 쓰레드가 각각 2만 개씩 나 누어서 처리하게 된다. 쓰레드 수가 5개를 초과하면 오히려 연산 시간이 증가하는 결과가 나타나, 총 쓰레드 수는 5개로 결정했다. 그러나 RANGE_PER_THREAD의 최적값은 추 가적인 연구를 통해 결정될 수 있을 것으로 보인다. MAX_NONCE는 다양한 실험을 통해, 주어진 프로젝트 범위 내에서 Nonce의 범위가 대체적으로 int 타입보다는 더 넓어 long 타입을 사용하는 것이 적합하다고 판단된다.각 쓰레드는 총 10만 개의 숫자 중 2만 개씩을 검사한다. 이 과정에서 전역변수인 stop_received가 false이고 found_nonce가 -1이 아닌 경우, 다른 워킹 서버 또는 쓰레 드에서 이미 nonce를 찾았다고 판단하여 연산을 중단한다. 그렇지 않은 경우, compute_SHA256 함수를 사용하여 해시 값을 계산하고, 해당 해시 값이 설정된 난이도 요구사항을 만족하는지 확인한다. 만약 만족하는 경우, 해당 해시 값을 화면에 출력하고, found_nonce를 현재의 my_nonce 값으로 변경하며, nonce를 찾은 쓰레드의 ID를 found_thread_id에 저장한다. 이러한 모든 작업은 각 쓰레드에서 동시에 실행되므로, 뮤 텍스를 사용하여 쓰레드 간의 상호 배제를 보장한다.

## 리스너 쓰레드

리스너 쓰레드는 message_listener 함수를 통해 main server에서 stop 메세지를 보내는지 항시 확인한다. stop 메세지가 온다면 stop_received를 true로 바꿈으로서 연산 쓰레드에게 stop 메세지를 알린다. 그리고 화면에 Stopped computation due to stop message을 출력 한다.
