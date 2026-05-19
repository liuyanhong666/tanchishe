#include<stdio.h>
#include<time.h>
#include<windows.h>
#include<stdlib.h>
#include<string.h>
#include<conio.h>

#define U 1
#define D 2
#define L 3
#define R 4

// 游戏模式
#define NORMAL_MODE 0
#define WALL_MODE 1
#define SPEED_MODE 2

typedef struct SNAKE {
    int x;
    int y;
    struct SNAKE* next;
}snake;

typedef struct OBSTACLE {
    int x;
    int y;
    struct OBSTACLE* next;
}obstacle;

typedef struct USER {
    int id;
    char username[50];
    char password[50];
    int highScore;
    int totalGames;
    int achievements[10];
}user;

typedef struct GAMELOG {
    int userId;
    char username[50];
    char startTime[30];
    int duration;
    int score;
    int difficulty;
    int maxCombo;
    int level;
}gamelog;

typedef struct USER_NODE {
    user data;
    struct USER_NODE* next;
}user_node;

typedef struct LOG_NODE {
    gamelog data;
    struct LOG_NODE* next;
}log_node;

// 道具类型
typedef enum {
    POWERUP_SPEED,      // 加速
    POWERUP_SLOW,       // 减速
    POWERUP_INVINCIBLE, // 无敌
    POWERUP_DOUBLE,     // 双倍分数
    POWERUP_SHRINK     // 缩短蛇身
}PowerUpType;

typedef struct POWERUP {
    int x;
    int y;
    PowerUpType type;
    int duration;
    int active;
    time_t startTime;
    time_t endTime;
}powerup;

// 全局变量
int score = 0, add = 10;
int status, sleeptime = 200;
snake* head = NULL, * food = NULL;
snake* q;
obstacle* obstacleList = NULL;
int endgamestatus = 0;
int gameMode = NORMAL_MODE;
int difficulty = 1;
int combo = 0;
int maxCombo = 0;

// 简化的效果变量 - 直接使用独立变量
int isInvincible = 0;        // 无敌状态
time_t invincibleEndTime;     // 无敌结束时间
int isDoubleScore = 0;        // 双倍分数
time_t doubleScoreEndTime;    // 双倍分数结束时间
int isSpeedUp = 0;           // 加速状态
time_t speedUpEndTime;        // 加速结束时间
int isSlowDown = 0;          // 减速状态
time_t slowDownEndTime;       // 减速结束时间

powerup activePowerUps[5];
int powerUpCount = 0;
int level = 1;
int foodsEaten = 0;
int gameRunning = 1;

// 上次吃到食物的时间，用于连击判断
time_t lastEatTime = 0;
#define COMBO_TIMEOUT 3  // 3秒内再次吃到食物算连击

char currentUser[50] = "";
int currentUserId = 0;
time_t gameStartTime;
user_node* userList = NULL;
log_node* logList = NULL;

// 函数声明
void Pos(int x, int y);
void SetColor(int color);
void creatMap();
void initsnake();
int biteself();
void createfood();
void createObstacles();
void createPowerUp();
void snakemove();
void pause();
void gamecircle();
void showMainMenu();
int endgame();
void gamestart();
void saveUserToFile();
void loadUserFromFile();
void saveLogToFile(gamelog* newLog);
void loadLogFromFile();
int registerUser();
int loginUser();
void showGameLog();
void showLeaderboard();
void showAchievements();
void recordGameLog(int finalScore);
void HideCursor();
void clearInputBuffer();
void drawUI();
void checkPowerUps();
void updatePowerUps();
void applyPowerUp(PowerUpType type);
void removePowerUp(int index);
void showHelp();
void selectDifficulty();
void updateAchievements();
void resetGame();
void checkAllEffects();
void updateCombo();
void displayEffectTimers();
void initEffects();

void HideCursor() {
    CONSOLE_CURSOR_INFO cursor_info = {1, 0};
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursor_info);
}

void clearInputBuffer() {
    while (_kbhit()) {
        _getch();
    }
}

void Pos(int x, int y) {
    COORD pos;
    HANDLE hOutput;
    pos.X = x;
    pos.Y = y;
    hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleCursorPosition(hOutput, pos);
}

void SetColor(int color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
}

// 初始化效果系统
void initEffects() {
    isInvincible = 0;
    isDoubleScore = 0;
    isSpeedUp = 0;
    isSlowDown = 0;
}

// 更新连击状态
void updateCombo() {
    if (combo > 0) {
        time_t currentTime = time(NULL);
        if (difftime(currentTime, lastEatTime) > COMBO_TIMEOUT) {
            combo = 0;
        }
    }
}

// 检查所有效果是否过期
void checkAllEffects() {
    time_t currentTime = time(NULL);

    if (isInvincible && currentTime >= invincibleEndTime) {
        isInvincible = 0;
    }

    if (isDoubleScore && currentTime >= doubleScoreEndTime) {
        isDoubleScore = 0;
    }

    if (isSpeedUp && currentTime >= speedUpEndTime) {
        isSpeedUp = 0;
        // 恢复正常速度
        switch(difficulty) {
            case 1: sleeptime = 250; break;
            case 2: sleeptime = 200; break;
            case 3: sleeptime = 150; break;
        }
    }

    if (isSlowDown && currentTime >= slowDownEndTime) {
        isSlowDown = 0;
        // 恢复正常速度
        switch(difficulty) {
            case 1: sleeptime = 250; break;
            case 2: sleeptime = 200; break;
            case 3: sleeptime = 150; break;
        }
    }
}

// 显示效果剩余时间
void displayEffectTimers() {
    time_t currentTime = time(NULL);
    int line = 12;
    int hasEffect = 0;

    // 清除之前的显示
    for (int i = 12; i <= 15; i++) {
        Pos(64, i);
        printf("                    ");
    }

    // 双倍分数
    if (isDoubleScore) {
        int remaining = (int)difftime(doubleScoreEndTime, currentTime);
        if (remaining > 0) {
            SetColor(14);  // 黄色
            Pos(64, line);
            printf("◆ 双倍分数: %2d秒", remaining);
            line++;
            hasEffect = 1;
        }
    }

    // 无敌状态
    if (isInvincible) {
        int remaining = (int)difftime(invincibleEndTime, currentTime);
        if (remaining > 0) {
            SetColor(12);  // 红色
            Pos(64, line);
            printf("★ 无敌状态: %2d秒", remaining);
            line++;
            hasEffect = 1;
        }
    }

    // 加速状态
    if (isSpeedUp) {
        int remaining = (int)difftime(speedUpEndTime, currentTime);
        if (remaining > 0) {
            SetColor(10);  // 绿色
            Pos(64, line);
            printf("▲ 加速状态: %2d秒", remaining);
            line++;
            hasEffect = 1;
        }
    }

    // 减速状态
    if (isSlowDown) {
        int remaining = (int)difftime(slowDownEndTime, currentTime);
        if (remaining > 0) {
            SetColor(9);  // 蓝色
            Pos(64, line);
            printf("▼ 减速状态: %2d秒", remaining);
            line++;
            hasEffect = 1;
        }
    }

    if (!hasEffect) {
        SetColor(8);
        Pos(64, 12);
        printf("暂无激活效果");
    }

    SetColor(7);
}

// 创建地图
void creatMap() {
    int i;
    SetColor(7);

    for (i = 0; i < 58; i += 2) {
        Pos(i, 0);
        printf("■");
        Pos(i, 26);
        printf("■");
    }
    for (i = 1; i < 26; i++) {
        Pos(0, i);
        printf("■");
        Pos(56, i);
        printf("■");
    }

    if (gameMode == WALL_MODE) {
        createObstacles();
    }
}

// 创建障碍物
void createObstacles() {
    obstacle* temp;
    int obstacleCount = difficulty * 3;

    while (obstacleList != NULL) {
        temp = obstacleList;
        obstacleList = obstacleList->next;
        free(temp);
    }

    for (int i = 0; i < obstacleCount; i++) {
        temp = (obstacle*)malloc(sizeof(obstacle));
        int valid = 0;

        while (!valid) {
            temp->x = (rand() % 25 + 2) * 2;
            temp->y = rand() % 23 + 2;

            valid = 1;
            obstacle* check = obstacleList;
            while (check != NULL) {
                if (check->x == temp->x && check->y == temp->y) {
                    valid = 0;
                    break;
                }
                check = check->next;
            }

            if (valid && head != NULL) {
                snake* s = head;
                while (s != NULL) {
                    if (s->x == temp->x && s->y == temp->y) {
                        valid = 0;
                        break;
                    }
                    s = s->next;
                }
            }
        }

        SetColor(8);
        Pos(temp->x, temp->y);
        printf("█");

        temp->next = obstacleList;
        obstacleList = temp;
    }
}

int hitObstacle() {
    obstacle* obs = obstacleList;
    while (obs != NULL) {
        if (head->x == obs->x && head->y == obs->y) {
            return 1;
        }
        obs = obs->next;
    }
    return 0;
}

// 创建道具
void createPowerUp() {
    if (powerUpCount >= 3) return;

    int chance = rand() % 100;
    if (chance < 20) {  // 20%概率生成道具
        powerup newPower;
        int valid = 0;

        while (!valid) {
            newPower.x = (rand() % 25 + 2) * 2;
            newPower.y = rand() % 23 + 2;
            newPower.type = (PowerUpType)(rand() % 5);
            newPower.duration = 10;
            newPower.active = 1;
            newPower.startTime = time(NULL);
            newPower.endTime = time(NULL) + 10;

            valid = 1;
            snake* s = head;
            while (s != NULL) {
                if (s->x == newPower.x && s->y == newPower.y) {
                    valid = 0;
                    break;
                }
                s = s->next;
            }

            if (food && food->x == newPower.x && food->y == newPower.y) {
                valid = 0;
            }

            obstacle* obs = obstacleList;
            while (obs != NULL) {
                if (obs->x == newPower.x && obs->y == newPower.y) {
                    valid = 0;
                    break;
                }
                obs = obs->next;
            }
        }

        activePowerUps[powerUpCount++] = newPower;

        // 在地图上显示道具
        SetColor(14);
        Pos(newPower.x, newPower.y);
        switch(newPower.type) {
            case POWERUP_SPEED: printf("▲"); break;
            case POWERUP_SLOW: printf("▼"); break;
            case POWERUP_INVINCIBLE: printf("★"); break;
            case POWERUP_DOUBLE: printf("◆"); break;
            case POWERUP_SHRINK: printf("●"); break;
        }
        SetColor(7);
    }
}

// 应用道具效果
void applyPowerUp(PowerUpType type) {
    time_t currentTime = time(NULL);

    switch(type) {
        case POWERUP_SPEED:
            sleeptime = 50;
            isSpeedUp = 1;
            speedUpEndTime = currentTime + 10;
            break;
        case POWERUP_SLOW:
            sleeptime = 300;
            isSlowDown = 1;
            slowDownEndTime = currentTime + 10;
            break;
        case POWERUP_INVINCIBLE:
            isInvincible = 1;
            invincibleEndTime = currentTime + 10;
            break;
        case POWERUP_DOUBLE:
            isDoubleScore = 1;
            doubleScoreEndTime = currentTime + 10;
            break;
        case POWERUP_SHRINK:
            {
                snake* current = head;
                int count = 0;
                while (current != NULL && count < 3) {
                    current = current->next;
                    count++;
                }
                if (current != NULL) {
                    while (current->next != NULL) {
                        snake* temp = current->next;
                        Pos(temp->x, temp->y);
                        printf("  ");
                        current->next = temp->next;
                        free(temp);
                    }
                }
            }
            break;
    }
}

// 更新道具状态
void updatePowerUps() {
    time_t currentTime = time(NULL);

    for (int i = powerUpCount - 1; i >= 0; i--) {
        if (activePowerUps[i].active) {
            if (currentTime >= activePowerUps[i].endTime) {
                Pos(activePowerUps[i].x, activePowerUps[i].y);
                printf("  ");
                removePowerUp(i);
            }
        }
    }

    static int powerUpTimer = 0;
    powerUpTimer++;
    if (powerUpTimer > 50) {
        createPowerUp();
        powerUpTimer = 0;
    }
}

void removePowerUp(int index) {
    for (int i = index; i < powerUpCount - 1; i++) {
        activePowerUps[i] = activePowerUps[i + 1];
    }
    powerUpCount--;
}

// 检查道具碰撞
void checkPowerUps() {
    for (int i = 0; i < powerUpCount; i++) {
        if (activePowerUps[i].active &&
            head->x == activePowerUps[i].x &&
            head->y == activePowerUps[i].y) {

            applyPowerUp(activePowerUps[i].type);

            Pos(activePowerUps[i].x, activePowerUps[i].y);
            printf("  ");

            removePowerUp(i);
            i--;
        }
    }
}

void initsnake() {
    snake* tail;
    int i;
    tail = (snake*)malloc(sizeof(snake));
    tail->x = 24;
    tail->y = 5;
    tail->next = NULL;

    for (i = 1; i <= 4; i++) {
        head = (snake*)malloc(sizeof(snake));
        head->next = tail;
        head->x = 24 + 2 * i;
        head->y = 5;
        tail = head;
    }

    SetColor(10);
    while (tail != NULL) {
        Pos(tail->x, tail->y);
        printf("■");
        tail = tail->next;
    }
}

int biteself() {
    // 无敌状态下不会咬到自己
    if (isInvincible) return 0;

    snake* self;
    self = head->next;
    while (self != NULL) {
        if (self->x == head->x && self->y == head->y) {
            return 1;
        }
        self = self->next;
    }
    return 0;
}

void createfood() {
    snake* food_1;
    int validPosition;

    food_1 = (snake*)malloc(sizeof(snake));

    do {
        validPosition = 1;
        food_1->x = rand() % 52 + 2;
        if (food_1->x % 2 != 0) {
            food_1->x++;
        }
        food_1->y = rand() % 24 + 1;

        q = head;
        while (q != NULL) {
            if (q->x == food_1->x && q->y == food_1->y) {
                validPosition = 0;
                break;
            }
            q = q->next;
        }

        if (validPosition && gameMode == WALL_MODE) {
            obstacle* obs = obstacleList;
            while (obs != NULL) {
                if (obs->x == food_1->x && obs->y == food_1->y) {
                    validPosition = 0;
                    break;
                }
                obs = obs->next;
            }
        }
    } while (!validPosition);

    food = food_1;

    if (rand() % 100 < 15) {
        SetColor(14);
        Pos(food->x, food->y);
        printf("★");
    } else {
        SetColor(12);
        Pos(food->x, food->y);
        printf("■");
    }
}

void snakemove() {
    snake* nexthead;

    // 必须先检查效果过期
    checkAllEffects();
    updateCombo();

    // 检查撞墙 - 无敌状态下不检测
    if (!isInvincible) {
        if (head->x == 0 || head->x == 56 || head->y == 0 || head->y == 26) {
            endgamestatus = 1;
            recordGameLog(score);
            if (!endgame()) exit(0);
            return;
        }

        // 检查撞障碍物
        if (gameMode == WALL_MODE && hitObstacle()) {
            endgamestatus = 4;
            recordGameLog(score);
            if (!endgame()) exit(0);
            return;
        }
    }

    nexthead = (snake*)malloc(sizeof(snake));

    if (status == U) {
        nexthead->x = head->x;
        nexthead->y = head->y - 1;
    } else if (status == D) {
        nexthead->x = head->x;
        nexthead->y = head->y + 1;
    } else if (status == L) {
        nexthead->x = head->x - 2;
        nexthead->y = head->y;
    } else {
        nexthead->x = head->x + 2;
        nexthead->y = head->y;
    }

    if (food && nexthead->x == food->x && nexthead->y == food->y) {
        // 吃到食物
        nexthead->next = head;
        head = nexthead;

        // 更新连击
        time_t currentTime = time(NULL);
        if (lastEatTime > 0 && difftime(currentTime, lastEatTime) <= COMBO_TIMEOUT) {
            combo++;
        } else {
            combo = 1;
        }
        lastEatTime = currentTime;

        if (combo > maxCombo) maxCombo = combo;

        // 计算分数
        int pointsEarned = add;
        if (isDoubleScore) pointsEarned *= 2;
        if (combo > 1) pointsEarned += combo * 2;

        score += pointsEarned;
        foodsEaten++;

        if (foodsEaten % 5 == 0) {
            level++;
            if (sleeptime > 50 && !isSpeedUp && !isSlowDown) {
                sleeptime -= 10;
            }
        }

        free(food);
        food = NULL;
        createfood();
    } else {
        // 没吃到食物
        updateCombo();

        nexthead->next = head;
        head = nexthead;

        q = head;
        while (q->next->next != NULL) {
            Pos(q->x, q->y);
            SetColor(10);
            printf("■");
            q = q->next;
        }
        Pos(q->next->x, q->next->y);
        printf("  ");
        free(q->next);
        q->next = NULL;
    }

    checkPowerUps();
    updatePowerUps();

    // 检查是否咬到自己
    if (biteself() == 1) {
        endgamestatus = 2;
        recordGameLog(score);
        if (!endgame()) exit(0);
        return;
    }
}

void pause() {
    SetColor(11);
    Pos(30, 14);
    printf("=== 游戏暂停 ===");
    Pos(30, 15);
    printf("按空格键继续");

    while (1) {
        Sleep(300);
        if (GetAsyncKeyState(VK_SPACE)) {
            Pos(30, 14);
            printf("                ");
            Pos(30, 15);
            printf("                ");
            break;
        }
    }
}

void drawUI() {
    // 更新效果状态
    checkAllEffects();

    SetColor(11);
    Pos(64, 2);
    printf("═══════════════════");
    Pos(64, 3);
    printf("  贪食蛇大作战");
    Pos(64, 4);
    printf("═══════════════════");

    SetColor(7);
    Pos(64, 6);
    printf("玩家: %s", currentUser);
    Pos(64, 8);
    SetColor(14);
    printf("得分: %d  ", score);
    Pos(64, 9);
    printf("等级: %d  ", level);

    Pos(64, 10);
    if (combo > 1) {
        SetColor(12);
        printf("连击: %d !!  ", combo);
    } else {
        SetColor(7);
        printf("连击: 0      ");
    }

    Pos(64, 11);
    SetColor(7);
    printf("食物分: %d  ", add);

    // 显示效果计时器
    displayEffectTimers();

    SetColor(7);
    Pos(64, 17);
    printf("控制说明:");
    Pos(64, 18);
    printf("↑↓←→ 移动");
    Pos(64, 19);
    printf("空格 暂停");
    Pos(64, 20);
    printf("F1 加速 F2 减速");
    Pos(64, 21);
    printf("F5 查看日志");
    Pos(64, 22);
    printf("ESC 退出游戏");

    SetColor(8);
    Pos(64, 24);
    printf("模式: ");
    switch(gameMode) {
        case NORMAL_MODE: printf("标准"); break;
        case WALL_MODE: printf("障碍"); break;
        case SPEED_MODE: printf("速度"); break;
    }

    Pos(64, 25);
    printf("难度: ");
    switch(difficulty) {
        case 1: printf("简单"); break;
        case 2: printf("普通"); break;
        case 3: printf("困难"); break;
    }

    SetColor(7);
}

void showHelp() {
    system("cls");
    SetColor(11);
    Pos(30, 5);
    printf("══════ 游戏帮助 ══════");

    SetColor(7);
    Pos(25, 8);
    printf("游戏模式:");
    Pos(25, 9);
    printf("1. 标准模式 - 经典贪食蛇玩法");
    Pos(25, 10);
    printf("2. 障碍模式 - 地图中有障碍物");

    Pos(25, 12);
    printf("连击系统:");
    Pos(25, 13);
    printf("  3秒内连续吃到食物触发连击");
    Pos(25, 14);
    printf("  连击数越高，额外加分越多");

    Pos(25, 16);
    printf("道具说明(持续10秒):");
    Pos(25, 17);
    printf("  ▲ 加速道具 - 临时增加速度");
    Pos(25, 18);
    printf("  ▼ 减速道具 - 临时降低速度");
    Pos(25, 19);
    printf("  ★ 无敌道具 - 10秒无敌(撞墙不死)");
    Pos(25, 20);
    printf("  ◆ 双倍分数 - 10秒双倍分数");
    Pos(25, 21);
    printf("  ● 缩短道具 - 缩短蛇身");

    SetColor(7);
    Pos(25, 23);
    printf("按任意键返回...");
    _getch();
}

void selectDifficulty() {
    system("cls");
    Pos(30, 10);
    printf("════ 选择难度 ════");
    Pos(30, 12);
    printf("1. 简单 (慢速)");
    Pos(30, 13);
    printf("2. 普通 (中速)");
    Pos(30, 14);
    printf("3. 困难 (快速)");
    Pos(30, 16);
    printf("请选择: ");

    int choice;
    scanf("%d", &choice);

    switch(choice) {
        case 1: difficulty = 1; sleeptime = 250; add = 8; break;
        case 2: difficulty = 2; sleeptime = 200; add = 10; break;
        case 3: difficulty = 3; sleeptime = 150; add = 15; break;
        default: difficulty = 2; sleeptime = 200; add = 10;
    }
}

void resetGame() {
    snake* current = head;
    while (current != NULL) {
        snake* temp = current;
        Pos(temp->x, temp->y);
        printf("  ");
        current = current->next;
        free(temp);
    }
    head = NULL;

    if (food != NULL) {
        Pos(food->x, food->y);
        printf("  ");
        free(food);
        food = NULL;
    }

    obstacle* obs = obstacleList;
    while (obs != NULL) {
        obstacle* temp = obs;
        Pos(temp->x, temp->y);
        printf("  ");
        obs = obs->next;
        free(temp);
    }
    obstacleList = NULL;

    for (int i = 0; i < powerUpCount; i++) {
        if (activePowerUps[i].active) {
            Pos(activePowerUps[i].x, activePowerUps[i].y);
            printf("  ");
        }
    }
    powerUpCount = 0;

    initEffects();

    score = 0;
    add = 10;
    combo = 0;
    maxCombo = 0;
    level = 1;
    foodsEaten = 0;
    status = R;
    endgamestatus = 0;
    lastEatTime = 0;

    switch(difficulty) {
        case 1: sleeptime = 250; add = 8; break;
        case 2: sleeptime = 200; add = 10; break;
        case 3: sleeptime = 150; add = 15; break;
    }

    system("cls");
    creatMap();
    initsnake();
    createfood();
    gameStartTime = time(NULL);
}

void updateAchievements() {
    user_node* p = userList;
    while (p != NULL) {
        if (strcmp(p->data.username, currentUser) == 0) {
            if (p->data.totalGames >= 1) p->data.achievements[0] = 1;
            if (score >= 100) p->data.achievements[1] = 1;
            if (score >= 500) p->data.achievements[2] = 1;
            if (maxCombo >= 10) p->data.achievements[3] = 1;
            if ((int)difftime(time(NULL), gameStartTime) >= 300) p->data.achievements[4] = 1;
            if (foodsEaten >= 50) p->data.achievements[5] = 1;
            if (difficulty == 3 && score >= 200) p->data.achievements[6] = 1;
            if (gameMode == WALL_MODE && score >= 300) p->data.achievements[7] = 1;
            if (score >= 1000) p->data.achievements[9] = 1;

            if (score > p->data.highScore) p->data.highScore = score;
            p->data.totalGames++;
            saveUserToFile();
            break;
        }
        p = p->next;
    }
}

void showMainMenu() {
    int choice;
    while (1) {
        system("cls");
        SetColor(11);
        Pos(35, 5);
        printf("╔══════════════════╗");
        Pos(35, 6);
        printf("║   贪食蛇大作战   ║");
        Pos(35, 7);
        printf("╚══════════════════╝");

        SetColor(7);
        Pos(35, 9); printf("1. 开始游戏");
        Pos(35, 10); printf("2. 选择模式");
        Pos(35, 11); printf("3. 选择难度");
        Pos(35, 12); printf("4. 排行榜");
        Pos(35, 13); printf("5. 游戏日志");
        Pos(35, 14); printf("6. 成就系统");
        Pos(35, 15); printf("7. 游戏帮助");
        Pos(35, 16); printf("8. 退出游戏");
        Pos(35, 18); printf("请选择: ");

        clearInputBuffer();
        scanf("%d", &choice);

        switch(choice) {
            case 1: return;
            case 2:
                system("cls");
                Pos(30, 10); printf("选择游戏模式:");
                Pos(30, 12); printf("1. 标准模式");
                Pos(30, 13); printf("2. 障碍模式");
                Pos(30, 15); printf("请选择: ");
                int mode;
                scanf("%d", &mode);
                if (mode >= 1 && mode <= 2) gameMode = mode - 1;
                break;
            case 3: selectDifficulty(); break;
            case 4: showLeaderboard(); break;
            case 5: showGameLog(); break;
            case 6: showAchievements(); break;
            case 7: showHelp(); break;
            case 8: exit(0);
        }
    }
}

void showLeaderboard() {
    system("cls");
    SetColor(14);
    Pos(30, 2);
    printf("══════ 排行榜 TOP 10 ══════");
    SetColor(7);
    Pos(20, 4); printf("排名  用户名        最高分  游戏次数");
    Pos(20, 5); printf("────────────────────────────────");

    user_node* users[100];
    int userCount = 0;
    user_node* p = userList;

    while (p != NULL && userCount < 100) {
        users[userCount++] = p;
        p = p->next;
    }

    for (int i = 0; i < userCount - 1; i++)
        for (int j = 0; j < userCount - 1 - i; j++)
            if (users[j]->data.highScore < users[j+1]->data.highScore) {
                user_node* temp = users[j];
                users[j] = users[j+1];
                users[j+1] = temp;
            }

    for (int i = 0; i < userCount && i < 10; i++) {
        Pos(20, 6 + i);
        if (i == 0) SetColor(14);
        else if (i == 1) SetColor(8);
        else if (i == 2) SetColor(6);
        else SetColor(7);
        printf("%-4d %-12s %-8d %-8d", i+1, users[i]->data.username,
               users[i]->data.highScore, users[i]->data.totalGames);
    }

    SetColor(7);
    Pos(20, 18);
    printf("按任意键返回...");
    _getch();
}

void showAchievements() {
    system("cls");
    SetColor(14);
    Pos(25, 2);
    printf("══════ 成就系统 ══════");
    SetColor(7);

    const char* achievements[] = {
        "初次游戏", "得分100", "得分500", "连击大师",
        "生存专家", "贪食者", "速度狂人", "障碍达人",
        "收集者", "满分王"
    };

    user_node* cur = userList;
    while (cur != NULL) {
        if (strcmp(cur->data.username, currentUser) == 0) break;
        cur = cur->next;
    }

    if (cur != NULL) {
        for (int i = 0; i < 10; i++) {
            Pos(20, 4 + i);
            if (cur->data.achievements[i]) {
                SetColor(10); printf("★ ");
            } else {
                SetColor(8); printf("☆ ");
            }
            printf("%s", achievements[i]);
        }
    }

    SetColor(7);
    Pos(20, 16);
    printf("按任意键返回...");
    _getch();
}

void saveUserToFile() {
    FILE* fp = fopen("users.dat", "wb");
    if (fp == NULL) return;
    int count = 0;
    user_node* p = userList;
    while (p != NULL) { count++; p = p->next; }
    fwrite(&count, sizeof(int), 1, fp);
    p = userList;
    while (p != NULL) {
        fwrite(&(p->data), sizeof(user), 1, fp);
        p = p->next;
    }
    fclose(fp);
}

void loadUserFromFile() {
    FILE* fp = fopen("users.dat", "rb");
    if (fp == NULL) return;
    int count = 0;
    if (fread(&count, sizeof(int), 1, fp) != 1) { fclose(fp); return; }
    user_node* p;
    for (int i = 0; i < count; i++) {
        p = (user_node*)malloc(sizeof(user_node));
        memset(&(p->data), 0, sizeof(user));
        if (fread(&(p->data), sizeof(user), 1, fp) == 1) {
            p->next = userList;
            userList = p;
        } else { free(p); break; }
    }
    fclose(fp);
}

void saveLogToFile(gamelog* newLog) {
    FILE* fp = fopen("gamelog.dat", "ab");
    if (fp == NULL) return;
    fwrite(newLog, sizeof(gamelog), 1, fp);
    fclose(fp);
}

void loadLogFromFile() {
    FILE* fp = fopen("gamelog.dat", "rb");
    if (fp == NULL) return;
    gamelog temp;
    log_node* p;
    while (logList != NULL) {
        log_node* tempNode = logList;
        logList = logList->next;
        free(tempNode);
    }
    while (fread(&temp, sizeof(gamelog), 1, fp) == 1) {
        p = (log_node*)malloc(sizeof(log_node));
        memset(&(p->data), 0, sizeof(gamelog));
        p->data = temp;
        p->next = logList;
        logList = p;
    }
    fclose(fp);
}

int registerUser() {
    system("cls");
    Pos(30, 10); printf("===== 用户注册 =====");
    char username[50], password[50], confirmPwd[50];
    Pos(30, 12); printf("请输入用户名：");
    clearInputBuffer();
    scanf("%49s", username);

    user_node* p = userList;
    while (p != NULL) {
        if (strcmp(p->data.username, username) == 0) {
            Pos(30, 16); printf("用户名已存在！");
            Sleep(1500);
            return 0;
        }
        p = p->next;
    }

    Pos(30, 13); printf("请输入密码：");
    scanf("%49s", password);
    Pos(30, 14); printf("请确认密码：");
    scanf("%49s", confirmPwd);

    if (strcmp(password, confirmPwd) != 0) {
        Pos(30, 16); printf("两次密码输入不一致！");
        Sleep(1500);
        return 0;
    }

    user_node* newUser = (user_node*)malloc(sizeof(user_node));
    memset(&(newUser->data), 0, sizeof(user));
    int maxId = 1000;
    p = userList;
    while (p != NULL) {
        if (p->data.id > maxId) maxId = p->data.id;
        p = p->next;
    }
    newUser->data.id = maxId + 1;
    strcpy(newUser->data.username, username);
    strcpy(newUser->data.password, password);
    newUser->next = userList;
    userList = newUser;
    saveUserToFile();

    Pos(30, 16); printf("注册成功！您的ID是：%d", newUser->data.id);
    Sleep(2000);
    return 1;
}

int loginUser() {
    system("cls");
    Pos(30, 10); printf("===== 用户登录 =====");
    char username[50], password[50];
    Pos(30, 12); printf("请输入用户名：");
    clearInputBuffer();
    scanf("%49s", username);
    Pos(30, 13); printf("请输入密码：");
    scanf("%49s", password);

    user_node* p = userList;
    while (p != NULL) {
        if (strcmp(p->data.username, username) == 0 &&
            strcmp(p->data.password, password) == 0) {
            strcpy(currentUser, username);
            currentUserId = p->data.id;
            Pos(30, 15); printf("登录成功！欢迎 %s", username);
            Sleep(1500);
            return 1;
        }
        p = p->next;
    }

    Pos(30, 15); printf("用户名或密码错误！");
    Sleep(1500);
    return 0;
}

void showGameLog() {
    system("cls");
    Pos(20, 2); printf("===== 游戏记录 =====");
    Pos(5, 4); printf("用户ID\t用户名\t\t开始时间\t\t时长(秒)\t得分\t模式\t最大连击");
    Pos(5, 5); printf("────────────────────────────────────────────────────────");

    int line = 6;
    log_node* p = logList;
    if (p == NULL) { Pos(20, 8); printf("暂无游戏记录！"); }

    while (p != NULL && line < 25) {
        Pos(5, line++);
        printf("%d\t%s\t\t%s\t%d\t\t%d\t%d\t%d",
               p->data.userId, p->data.username, p->data.startTime,
               p->data.duration, p->data.score, p->data.difficulty, p->data.maxCombo);
        p = p->next;
    }

    Pos(20, 26); printf("按任意键返回...");
    clearInputBuffer();
    _getch();
}

void recordGameLog(int finalScore) {
    gamelog newLog;
    memset(&newLog, 0, sizeof(gamelog));
    newLog.userId = currentUserId;
    strcpy(newLog.username, currentUser);
    struct tm* timeinfo = localtime(&gameStartTime);
    strftime(newLog.startTime, sizeof(newLog.startTime), "%Y-%m-%d %H:%M:%S", timeinfo);
    time_t endTime = time(NULL);
    newLog.duration = (int)difftime(endTime, gameStartTime);
    newLog.score = finalScore;
    newLog.difficulty = difficulty;
    newLog.maxCombo = maxCombo;
    newLog.level = level;

    log_node* newNode = (log_node*)malloc(sizeof(log_node));
    newNode->data = newLog;
    newNode->next = logList;
    logList = newNode;
    saveLogToFile(&newLog);
    updateAchievements();
}

int endgame() {
    system("cls");
    SetColor(12);
    Pos(24, 8);
    if (endgamestatus == 1) printf("对不起，您撞到墙了。");
    else if (endgamestatus == 2) printf("对不起，您咬到自己了。");
    else if (endgamestatus == 3) printf("您主动结束了游戏。");
    else if (endgamestatus == 4) printf("您撞到了障碍物。");

    SetColor(11);
    Pos(24, 10); printf("══════ 游戏统计 ══════");
    SetColor(7);
    Pos(24, 12); printf("最终得分：%d", score);
    Pos(24, 13); printf("游戏等级：%d", level);
    Pos(24, 14); printf("游戏时长：%d秒", (int)difftime(time(NULL), gameStartTime));
    Pos(24, 15); printf("最大连击：%d", maxCombo);
    Pos(24, 16); printf("吃掉食物：%d个", foodsEaten);

    user_node* p = userList;
    int isNewRecord = 0;
    while (p != NULL) {
        if (strcmp(p->data.username, currentUser) == 0) {
            if (score > p->data.highScore) isNewRecord = 1;
            break;
        }
        p = p->next;
    }

    if (isNewRecord && score > 0) {
        SetColor(14);
        Pos(24, 18); printf("★ 恭喜！新纪录！ ★");
    }

    SetColor(11);
    Pos(24, 20); printf("════════════════════");
    Pos(24, 21); printf("请选择：");
    SetColor(10);
    Pos(24, 22); printf("1. 再来一局");
    SetColor(12);
    Pos(24, 23); printf("2. 返回主菜单");
    SetColor(8);
    Pos(24, 24); printf("3. 退出游戏");
    SetColor(7);
    Pos(24, 26); printf("请输入选择：");

    clearInputBuffer();
    int choice;
    scanf("%d", &choice);

    switch(choice) {
        case 1:
            resetGame();
            return 1;
        case 2:
            resetGame();
            showMainMenu();
            system("cls");
            creatMap();
            initsnake();
            createfood();
            gameStartTime = time(NULL);
            return 1;
        case 3:
            return 0;
        default:
            resetGame();
            showMainMenu();
            system("cls");
            creatMap();
            initsnake();
            createfood();
            gameStartTime = time(NULL);
            return 1;
    }
}

void gamecircle() {
    gameStartTime = time(NULL);
    srand((unsigned)time(NULL));
    lastEatTime = 0;
    initEffects();

    status = R;
    while (1) {
        drawUI();

        if (GetAsyncKeyState(VK_UP) && status != D) status = U;
        else if (GetAsyncKeyState(VK_DOWN) && status != U) status = D;
        else if (GetAsyncKeyState(VK_LEFT) && status != R) status = L;
        else if (GetAsyncKeyState(VK_RIGHT) && status != L) status = R;
        else if (GetAsyncKeyState(VK_SPACE)) pause();
        else if (GetAsyncKeyState(VK_ESCAPE)) {
            endgamestatus = 3;
            recordGameLog(score);
            if (!endgame()) exit(0);
        }
        else if (GetAsyncKeyState(VK_F1)) {
            if (sleeptime >= 50 && !isSpeedUp && !isSlowDown) {
                sleeptime -= 30;
                add += 2;
            }
        }
        else if (GetAsyncKeyState(VK_F2)) {
            if (sleeptime < 350 && !isSpeedUp && !isSlowDown) {
                sleeptime += 30;
                add -= 2;
                if (add < 1) add = 1;
            }
        }
        else if (GetAsyncKeyState(VK_F5)) {
            showGameLog();
            system("cls");
            creatMap();
            q = head;
            while (q != NULL) {
                Pos(q->x, q->y);
                SetColor(10);
                printf("■");
                q = q->next;
            }
            if (food != NULL) {
                Pos(food->x, food->y);
                SetColor(12);
                printf("■");
            }
        }
        Sleep(sleeptime);
        snakemove();
    }
}

void welcometogame() {
    loadUserFromFile();
    loadLogFromFile();

    int choice;
    while (1) {
        system("cls");
        SetColor(11);
        Pos(35, 8); printf("╔══════════════════╗");
        Pos(35, 9); printf("║   贪食蛇大作战   ║");
        Pos(35, 10); printf("╚══════════════════╝");
        SetColor(7);
        Pos(35, 12); printf("1. 用户登录");
        Pos(35, 13); printf("2. 用户注册");
        Pos(35, 14); printf("3. 退出游戏");
        Pos(35, 16); printf("请选择：");
        clearInputBuffer();
        scanf("%d", &choice);

        if (choice == 1) { if (loginUser()) break; }
        else if (choice == 2) registerUser();
        else if (choice == 3) exit(0);
    }

    showMainMenu();
    system("cls");
}

void gamestart() {
    system("mode con cols=100 lines=30");
    HideCursor();
    initEffects();
    welcometogame();
    creatMap();
    initsnake();
    createfood();
}

int main() {
    gamestart();
    gamecircle();
    endgame();
    return 0;
}
