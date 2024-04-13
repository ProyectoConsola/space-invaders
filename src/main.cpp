#include "fabgl.h"
#include "fabutils.h"
#include <Ps3Controller.h> 
#include "sprites.h"
#include "sounds.h"
#include "WiFiGeneric.h" 
 
 
 
using fabgl::iclamp;
 
 
fabgl::VGAController DisplayController;
fabgl::Canvas        canvas(&DisplayController);
SoundGenerator       soundGenerator;


// IntroScene
 
struct IntroScene : public Scene {
 
  static const int TEXTROWS = 4;
  static const int TEXT_X   = 130;
  static const int TEXT_Y   = 122;
 
 
  int textRow_  = 0;
  int textCol_  = 0;
  int starting_ = 0;
 
  SamplesGenerator * music_ = nullptr;
 
  IntroScene()
    : Scene(0, 20, DisplayController.getViewPortWidth(), DisplayController.getViewPortHeight())
  {
  }
 
  void init()
  {
    canvas.setBrushColor(Color::Black);
    canvas.clear();
    canvas.setGlyphOptions(GlyphOptions().FillBackground(true));
    canvas.selectFont(&fabgl::FONT_8x8);
    canvas.setPenColor(Color::BrightWhite);
    canvas.setGlyphOptions(GlyphOptions().DoubleWidth(1));
    canvas.drawText(50, 15, "SPACE INVADERS");
    canvas.setGlyphOptions(GlyphOptions().DoubleWidth(0));
 
    canvas.setPenColor(Color::Cyan);
    canvas.drawText(80, 40, "con ESP32 por FIE");
    canvas.drawText(30, 55, "Facultad de Ingenieria Electrica.");
 
    canvas.setPenColor(Color::Yellow);
    canvas.setBrushColor(0, 0, 0);
    canvas.fillRectangle(70, 92, 240, 110);
    canvas.drawRectangle(70, 92, 240, 110);
    canvas.setPenColor(Color::Yellow);
    canvas.drawText(72, 97, "  Tabla de puntajes  ");
    canvas.drawBitmap(TEXT_X - 40 - 2, TEXT_Y - 2, &bmpEnemyD);
    canvas.drawBitmap(TEXT_X - 40, TEXT_Y + 10, &bmpEnemyA[0]);
    canvas.drawBitmap(TEXT_X - 40, TEXT_Y + 25, &bmpEnemyB[0]);
    canvas.drawBitmap(TEXT_X - 40, TEXT_Y + 40, &bmpEnemyC[0]);
 
    canvas.setBrushColor(Color::Black);

 
 
    music_ = soundGenerator.playSamples(themeSoundSamples, sizeof(themeSoundSamples), 100, -1);
  }
 
  void update(int updateCount)
  {
    static const char * scoreText[] = {"= ? MISTERIOSO", "= 30 PUNTOS", "= 20 PUNTOS", "= 10 PUNTOS" };
 


    if (starting_) {
 
      if (starting_ > 50) {
        // stop music
        soundGenerator.detach(music_);
        // stop scene
        stop();
      }
 
      ++starting_;
      canvas.scroll(0, -5);
 
    } else {
      if (updateCount > 30 && updateCount % 5 == 0 && textRow_ < 4) {
        int x = TEXT_X + textCol_ * canvas.getFontInfo()->width - 9;
        int y = TEXT_Y + textRow_ * 15 - 4;
        canvas.setPenColor(Color::White);
        canvas.drawChar(x, y, scoreText[textRow_][textCol_]);
        ++textCol_;
        if (scoreText[textRow_][textCol_] == 0) {
          textCol_ = 0;
          ++textRow_;
        }
      }
 
       if (updateCount % 20 == 0) {
        canvas.setPenColor(51, random(255), random(255));
        canvas.drawText(50, 75, "Presiona [START] para jugar");
      }

 
      // handle keyboard or mouse (after two seconds)
      if (updateCount > 50) {
        if (Ps3.event.button_down.start)
        starting_ = true; 
      }
    }
  }
 
  void collisionDetected(Sprite * spriteA, Sprite * spriteB, Point collisionPoint)
  {
  }
 
};
 
// GameScene
struct GameScene : public Scene {
 
  enum SpriteType { TYPE_PLAYERFIRE, TYPE_ENEMIESFIRE, TYPE_ENEMY, TYPE_PLAYER, TYPE_SHIELD, TYPE_ENEMYMOTHER };
 
  struct SISprite : Sprite {
    SpriteType type;
    uint8_t    enemyPoints;
  };
 
  enum GameState { GAMESTATE_PLAYING, GAMESTATE_PLAYERKILLED, GAMESTATE_ENDGAME, GAMESTATE_GAMEOVER, GAMESTATE_LEVELCHANGING, GAMESTATE_LEVELCHANGED };
 
  static const int PLAYERSCOUNT       = 1;
  static const int SHIELDSCOUNT       = 4;
  static const int ROWENEMIESCOUNT    = 11;
  static const int PLAYERFIRECOUNT    = 1;
  static const int ENEMIESFIRECOUNT   = 1;
  static const int ENEMYMOTHERCOUNT   = 1;
  static const int SPRITESCOUNT       = PLAYERSCOUNT + SHIELDSCOUNT + 5 * ROWENEMIESCOUNT + PLAYERFIRECOUNT + ENEMIESFIRECOUNT + ENEMYMOTHERCOUNT;
 
  static const int ENEMIES_X_SPACE    = 16; //Espacio entre enemigos
  static const int ENEMIES_Y_SPACE    = 10;
  static const int ENEMIES_START_X    = 0;
  static const int ENEMIES_START_Y    = 30;
  static const int ENEMIES_STEP_X     = 6;
  static const int ENEMIES_STEP_Y     = 8;
 
  static const int PLAYER_Y           = 170;
 
  static int lives_;
  static int score_;
  static int level_;
  static int hiScore_;
 
  SISprite * sprites_     = new SISprite[SPRITESCOUNT];
  SISprite * player_      = sprites_;
  SISprite * shields_     = player_ + PLAYERSCOUNT;
  SISprite * enemies_     = shields_ + SHIELDSCOUNT;
  SISprite * enemiesR1_   = enemies_;
  SISprite * enemiesR2_   = enemiesR1_ + ROWENEMIESCOUNT;
  SISprite * enemiesR3_   = enemiesR2_ + ROWENEMIESCOUNT;
  SISprite * enemiesR4_   = enemiesR3_ + ROWENEMIESCOUNT;
  SISprite * enemiesR5_   = enemiesR4_ + ROWENEMIESCOUNT;
  SISprite * playerFire_  = enemiesR5_ + ROWENEMIESCOUNT;
  SISprite * enemiesFire_ = playerFire_ + PLAYERFIRECOUNT;
  SISprite * enemyMother_ = enemiesFire_ + ENEMIESFIRECOUNT;
 
  int playerVelX_          = 0;  // 0 = no move
  int enemiesX_            = ENEMIES_START_X;
  int enemiesY_            = ENEMIES_START_Y;
 
  // enemiesDir_
  //   bit 0 : if 1 moving left
  //   bit 1 : if 1 moving right
  //   bit 2 : if 1 moving down
  //   bit 3 : if 0 before was moving left, if 1 before was moving right
  // Allowed cases:
  //   1  = moving left
  //   2  = moving right
  //   4  = moving down (before was moving left)
  //   12 = moving down (before was moving right)
 
  static constexpr int ENEMY_MOV_LEFT              = 1;
  static constexpr int ENEMY_MOV_RIGHT             = 2;
  static constexpr int ENEMY_MOV_DOWN_BEFORE_LEFT  = 4;
  static constexpr int ENEMY_MOV_DOWN_BEFORE_RIGHT = 12;
 
  int enemiesDir_          = ENEMY_MOV_RIGHT;
 
  int enemiesAlive_        = ROWENEMIESCOUNT * 5;
  int enemiesSoundCount_   = 0;
  SISprite * lastHitEnemy_ = nullptr;
  GameState gameState_     = GAMESTATE_PLAYING;
 
  bool updateScore_        = true;
  int64_t pauseStart_;
 
  Bitmap bmpShield[4] = { Bitmap(22, 16, shield_data, PixelFormat::Mask, RGB888(0, 255, 0), true),
                          Bitmap(22, 16, shield_data, PixelFormat::Mask, RGB888(0, 255, 0), true),
                          Bitmap(22, 16, shield_data, PixelFormat::Mask, RGB888(0, 255, 0), true),
                          Bitmap(22, 16, shield_data, PixelFormat::Mask, RGB888(0, 255, 0), true), };
 
  GameScene()
    : Scene(SPRITESCOUNT, 20, DisplayController.getViewPortWidth(), DisplayController.getViewPortHeight())
  {
  }
 
  ~GameScene()
  {
    delete [] sprites_;
  }
 
  void initEnemy(Sprite * sprite, int points)
  {
    SISprite * s = (SISprite*) sprite;
    s->addBitmap(&bmpEnemyExplosion);
    s->type = TYPE_ENEMY;
    s->enemyPoints = points;
    addSprite(s);
  }
 
  void init()
  {
    // setup player
    player_->addBitmap(&bmpPlayer)->addBitmap(&bmpPlayerExplosion[0])->addBitmap(&bmpPlayerExplosion[1]);
    player_->moveTo(152, PLAYER_Y);
    player_->type = TYPE_PLAYER;
    addSprite(player_);
    // setup player fire
    playerFire_->addBitmap(&bmpPlayerFire);
    playerFire_->visible = false;
    playerFire_->type = TYPE_PLAYERFIRE;
    addSprite(playerFire_);
    // setup shields
    for (int i = 0; i < 4; ++i) {
      shields_[i].addBitmap(&bmpShield[i])->moveTo(35 + i * 75, 150);
      shields_[i].isStatic = true;
      shields_[i].type = TYPE_SHIELD;
      addSprite(&shields_[i]);
    }
    // setup enemies
    for (int i = 0; i < ROWENEMIESCOUNT; ++i) {
      initEnemy( enemiesR1_[i].addBitmap(&bmpEnemyA[0])->addBitmap(&bmpEnemyA[1]), 30 );
      initEnemy( enemiesR2_[i].addBitmap(&bmpEnemyB[0])->addBitmap(&bmpEnemyB[1]), 20 );
      initEnemy( enemiesR3_[i].addBitmap(&bmpEnemyB[0])->addBitmap(&bmpEnemyB[1]), 20 );
      initEnemy( enemiesR4_[i].addBitmap(&bmpEnemyC[0])->addBitmap(&bmpEnemyC[1]), 10 );
      initEnemy( enemiesR5_[i].addBitmap(&bmpEnemyC[0])->addBitmap(&bmpEnemyC[1]), 10 );
    }
    // setup enemies fire
    enemiesFire_->addBitmap(&bmpEnemiesFire[0])->addBitmap(&bmpEnemiesFire[1]);
    enemiesFire_->visible = false;
    enemiesFire_->type = TYPE_ENEMIESFIRE;
    addSprite(enemiesFire_);
    // setup enemy mother ship
    enemyMother_->addBitmap(&bmpEnemyD)->addBitmap(&bmpEnemyExplosionRed);
    enemyMother_->visible = false;
    enemyMother_->type = TYPE_ENEMYMOTHER;
    enemyMother_->enemyPoints = 100;
    enemyMother_->moveTo(getWidth(), ENEMIES_START_Y);
    addSprite(enemyMother_);
 
    DisplayController.setSprites(sprites_, SPRITESCOUNT);



    canvas.setBrushColor(Color::Black);
    canvas.clear();
 
    canvas.setPenColor(Color::Green);
    canvas.drawLine(0, 180, 320, 180);
 

    canvas.setGlyphOptions(GlyphOptions().FillBackground(true));
    canvas.selectFont(&fabgl::FONT_4x6);
    canvas.setPenColor(Color::White);
    canvas.drawText(110, 20, "Bienvenido al espacio");
    canvas.selectFont(&fabgl::FONT_8x8);
    canvas.setPenColor(0, 255, 255);
    canvas.drawText(2, 2, "SCORE");
    canvas.setPenColor(0, 0, 255);
    canvas.drawText(254, 2, "HI-SCORE");
    canvas.setPenColor(255, 255, 255);
    canvas.drawTextFmt(254, 181, "Nivel %02d", level_);
 
    showLives();
  }
 
  void drawScore()
  {
    canvas.setPenColor(255, 255, 255);
    canvas.drawTextFmt(2, 14, "%05d", score_);
    if (score_ > hiScore_)
      hiScore_ = score_;
    canvas.setPenColor(255, 255, 255);
    canvas.drawTextFmt(266, 14, "%05d", hiScore_);
  }
 
  void moveEnemy(SISprite * enemy, int x, int y, bool * touchSide)
  {
    if (enemy->visible) {
      if (x <= 0 || x >= getWidth() - enemy->getWidth())
        *touchSide = true;
      enemy->moveTo(x, y);
      enemy->setFrame(enemy->getFrameIndex() ? 0 : 1);
      updateSprite(enemy);
      if (y >= PLAYER_Y) {
        // enemies reach earth!
        gameState_ = GAMESTATE_ENDGAME;
      }
    }
  }
 
  void gameOver()
  {
    // disable enemies drawing, so text can be over them
    for (int i = 0; i < ROWENEMIESCOUNT * 5; ++i)
      enemies_[i].allowDraw = false;
    // show game over
    canvas.setPenColor(0, 255, 0);
    canvas.setBrushColor(0, 0, 0);
    canvas.fillRectangle(40, 60, 270, 130);
    canvas.drawRectangle(40, 60, 270, 130);
    canvas.setGlyphOptions(GlyphOptions().DoubleWidth(1));
    canvas.setPenColor(255, 255, 255);
    canvas.drawText(55, 80, "FIN DEL JUEGO");
    canvas.setGlyphOptions(GlyphOptions().DoubleWidth(0));
    canvas.setPenColor(0, 255, 0);
    canvas.drawText(110, 100, "Presiona [X]");
    // change state
    gameState_ = GAMESTATE_GAMEOVER;
    level_ = 1;
    lives_ = 3;
    score_ = 0;
  }
 
  void levelChange()
  {
    ++level_;
    // show game over
    canvas.setPenColor(0, 255, 0);
    canvas.drawRectangle(80, 80, 240, 110);
    canvas.setGlyphOptions(GlyphOptions().DoubleWidth(1));
    canvas.drawTextFmt(105, 88, "NIVEL %d", level_);
    canvas.setGlyphOptions(GlyphOptions().DoubleWidth(0));
    // change state
    gameState_  = GAMESTATE_LEVELCHANGED;
    pauseStart_ = esp_timer_get_time();
  }
 
  void update(int updateCount)
  {
 
    if (updateScore_) {
      updateScore_ = false;
      drawScore();
    }
 
    if (gameState_ == GAMESTATE_PLAYING || gameState_ == GAMESTATE_PLAYERKILLED) {
 
      // move enemies and shoot
      if ((updateCount % max(3, 21 - level_ * 2)) == 0) {
        // handle enemy explosion
        if (lastHitEnemy_) {
          lastHitEnemy_->visible = false;
          lastHitEnemy_ = nullptr;
        }
        // handle enemies movement
        enemiesX_ += (-1 * (enemiesDir_ & 1) + (enemiesDir_ >> 1 & 1)) * ENEMIES_STEP_X;
        enemiesY_ += (enemiesDir_ >> 2 & 1) * ENEMIES_STEP_Y;
        bool touchSide = false;
        for (int i = 0; i < ROWENEMIESCOUNT; ++i) {
          moveEnemy(&enemiesR1_[i], enemiesX_ + i * ENEMIES_X_SPACE, enemiesY_ + 0 * ENEMIES_Y_SPACE, &touchSide);
          moveEnemy(&enemiesR2_[i], enemiesX_ + i * ENEMIES_X_SPACE, enemiesY_ + 1 * ENEMIES_Y_SPACE, &touchSide);
          moveEnemy(&enemiesR3_[i], enemiesX_ + i * ENEMIES_X_SPACE, enemiesY_ + 2 * ENEMIES_Y_SPACE, &touchSide);
          moveEnemy(&enemiesR4_[i], enemiesX_ + i * ENEMIES_X_SPACE, enemiesY_ + 3 * ENEMIES_Y_SPACE, &touchSide);
          moveEnemy(&enemiesR5_[i], enemiesX_ + i * ENEMIES_X_SPACE, enemiesY_ + 4 * ENEMIES_Y_SPACE, &touchSide);
        }
        switch (enemiesDir_) {
          case ENEMY_MOV_DOWN_BEFORE_LEFT:
            enemiesDir_ = ENEMY_MOV_RIGHT;
            break;
          case ENEMY_MOV_DOWN_BEFORE_RIGHT:
            enemiesDir_ = ENEMY_MOV_LEFT;
            break;
          default:
            if (touchSide)
              enemiesDir_ = (enemiesDir_ == ENEMY_MOV_LEFT ? ENEMY_MOV_DOWN_BEFORE_LEFT : ENEMY_MOV_DOWN_BEFORE_RIGHT);
            break;
        }
        // sound
        ++enemiesSoundCount_;
        soundGenerator.playSamples(invadersSoundSamples[enemiesSoundCount_ % 4], invadersSoundSamplesSize[enemiesSoundCount_ % 4]);
        // handle enemies fire generation
        if (!enemiesFire_->visible) {
          int shottingEnemy = random(enemiesAlive_);
          for (int i = 0, a = 0; i < ROWENEMIESCOUNT * 5; ++i) {
            if (enemies_[i].visible) {
              if (a == shottingEnemy) {
                enemiesFire_->x = enemies_[i].x + enemies_[i].getWidth() / 2;
                enemiesFire_->y = enemies_[i].y + enemies_[i].getHeight() / 2;
                enemiesFire_->visible = true;
                break;
              }
              ++a;
            }
          }
        }
      }
 
      if (gameState_ == GAMESTATE_PLAYERKILLED) {
        // animate player explosion or restart playing other lives
        if ((updateCount % 20) == 0) {
          if (player_->getFrameIndex() == 1) {
            player_->setFrame(2);
            Serial.println("Cambio de animación de 1 a 2");
          }
          else {
            player_->setFrame(0);
            gameState_ = GAMESTATE_PLAYING;
            Serial.println("Cambio de animación de 2 a 0");
          }
        }
      } else if (playerVelX_ != 0) {
        // move player using PS3
        player_->x += playerVelX_;
        player_->x = iclamp(player_->x, 0, getWidth() - player_->getWidth());
        updateSprite(player_);
      } 
 
      // move player fire
      if (playerFire_->visible) {
        playerFire_->y -= 3;
        if (playerFire_->y < ENEMIES_START_Y)
          playerFire_->visible = false;
        else
          updateSpriteAndDetectCollisions(playerFire_);
      }
 
      // move enemies fire
      if (enemiesFire_->visible) {
        enemiesFire_->y += 2;
        enemiesFire_->setFrame( enemiesFire_->getFrameIndex() ? 0 : 1 );
        if (enemiesFire_->y > PLAYER_Y + player_->getHeight())
          enemiesFire_->visible = false;
        else
          updateSpriteAndDetectCollisions(enemiesFire_);
      }
 
      // move enemy mother ship
      if (enemyMother_->visible && enemyMother_->getFrameIndex() == 0) {
        enemyMother_->x -= 1;
        if (enemyMother_->x < -enemyMother_->getWidth())
          enemyMother_->visible = false;
        else
          updateSprite(enemyMother_);
      }
 
      // start enemy mother ship
      if ((updateCount % 800) == 0) {
        soundGenerator.playSamples(motherShipSoundSamples, sizeof(motherShipSoundSamples), 100, 7000);
        enemyMother_->x = getWidth();
        enemyMother_->setFrame(0);
        enemyMother_->visible = true;
      }
 
      //Uso del control de PS3. 
      if (Ps3.data.analog.stick.rx > 90 || Ps3.data.analog.stick.rx < -90) {
        if (Ps3.data.analog.stick.rx > 90) {
            playerVelX_ = +1;
        }else if (Ps3.data.analog.stick.rx < -90) {
        playerVelX_ = -1;
        }
      } 
      else {
        playerVelX_ = 0;
      }


      if (abs(Ps3.event.analog_changed.button.cross) && !playerFire_->visible)  // player fire?
          fire(); 
      }
 
    if (gameState_ == GAMESTATE_ENDGAME)
      gameOver();
 
    if (gameState_ == GAMESTATE_LEVELCHANGING)
      levelChange();
 
    if (gameState_ == GAMESTATE_LEVELCHANGED && esp_timer_get_time() >= pauseStart_ + 2500000) {
      stop(); // restart from next level
      DisplayController.removeSprites();
    }
 
    if (gameState_ == GAMESTATE_GAMEOVER) {
 
      // animate player burning
      if ((updateCount % 20) == 0)
        player_->setFrame( player_->getFrameIndex() == 1 ? 2 : 1);
 
  
      if (Ps3.event.button_down.cross) {
        stop();
        DisplayController.removeSprites();
      }
 
    }
    DisplayController.refreshSprites();
    Serial.println("Update terminado");
  }
 
  // player shoots
  void fire()
  {
    playerFire_->moveTo(player_->x + 7, player_->y - 1)->visible = true;
    soundGenerator.playSamples(fireSoundSamples, sizeof(fireSoundSamples));
  }
 
  // shield has been damaged
  void damageShield(SISprite * shield, Point collisionPoint)
  {
    Bitmap * shieldBitmap = shield->getFrame();
    int x = collisionPoint.X - shield->x;
    int y = collisionPoint.Y - shield->y;
    shieldBitmap->setPixel(x, y, 0);
    for (int i = 0; i < 32; ++i) {
      int px = iclamp(x + random(-4, 5), 0, shield->getWidth() - 1);
      int py = iclamp(y + random(-4, 5), 0, shield->getHeight() - 1);
      shieldBitmap->setPixel(px, py, 0);
    }
  }
 
  void showLives()
  {
    canvas.fillRectangle(1, 181, 100, 195);
    canvas.setPenColor(Color::White);
    canvas.drawTextFmt(5, 181, "%d", lives_);
    for (int i = 0; i < lives_; ++i)
      canvas.drawBitmap(15 + i * (bmpPlayer.width + 5), 183, &bmpPlayer);
  }
 
  void collisionDetected(Sprite * spriteA, Sprite * spriteB, Point collisionPoint)
  {
    SISprite * sA = (SISprite*) spriteA;
    SISprite * sB = (SISprite*) spriteB;
    if (!lastHitEnemy_ && sA->type == TYPE_PLAYERFIRE && sB->type == TYPE_ENEMY) {
      // player fire hits an enemy
      soundGenerator.playSamples(shootSoundSamples, sizeof(shootSoundSamples));
      sA->visible = false;
      sB->setFrame(2);
      lastHitEnemy_ = sB;
      --enemiesAlive_;
      score_ += sB->enemyPoints;
      updateScore_ = true;
      if (enemiesAlive_ == 0)
        gameState_ = GAMESTATE_LEVELCHANGING;
    }
    if (sB->type == TYPE_SHIELD) {
      // something hits a shield
      sA->visible = false;
      damageShield(sB, collisionPoint);
      sB->allowDraw = true;
    }
    if (gameState_ == GAMESTATE_PLAYING) {
      // enemies fire hits player
      soundGenerator.playSamples(explosionSoundSamples, sizeof(explosionSoundSamples));
      --lives_;
      gameState_ = lives_ ? GAMESTATE_PLAYERKILLED : GAMESTATE_ENDGAME;
      player_->setFrame(1);
      showLives();
      Serial.println("Emitiendo colisión fuego jugador");
    }
    if (sB->type == TYPE_ENEMYMOTHER) {
      // player fire hits enemy mother ship
      soundGenerator.playSamples(mothershipexplosionSoundSamples, sizeof(mothershipexplosionSoundSamples));
      sA->visible = false;
      sB->setFrame(1);
      lastHitEnemy_ = sB;
      score_ += sB->enemyPoints;
      updateScore_ = true;
    }
    Serial.println("Colisiones procesadas");
  }
 
};
 
int GameScene::hiScore_ = 0;
int GameScene::level_   = 1;
int GameScene::lives_   = 3;
int GameScene::score_   = 0;
 
 
 
 
 
void setup()
{
  Serial.begin(115200);
  Ps3.begin("78:dd:08:4d:94:a4");
  DisplayController.begin();
  DisplayController.setResolution(VGA_320x200_75Hz);
  while(!Ps3.isConnected()) {
    delay(1000);
  }
}
 
 
void loop()
{
  GameScene gameScene;
  gameScene.start();
}