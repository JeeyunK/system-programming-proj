# IC619 고급 시스템 프로그래밍 프로젝트
김지윤, 한소영

## title
compression과 encryption을 지원하는 block device 구현

## introduction
1. compression 알고리즘을 사용하여 압축 전 data와 압축 후 data의 크기를 비교하여 압축률 확인
2. encryption 알고리즘을 사용하여 data 암호화. (ex: xor cipher)



## theory
- Dedup compression

- xor cipher

## Metrics of success
- 10000개의 random한 크기를 가진 파일을 생성해 압축률 확인
- correct key, incorrect key를 사용하여 encryption이 잘 이루어졌는지 확인
